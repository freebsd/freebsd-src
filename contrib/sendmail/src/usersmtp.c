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
static char id[] = "@(#)$Id: usersmtp.c,v 8.245.4.33 2001/05/23 18:53:09 ca Exp $ (with SMTP)";
# else /* SMTP */
static char id[] = "@(#)$Id: usersmtp.c,v 8.245.4.33 2001/05/23 18:53:09 ca Exp $ (without SMTP)";
# endif /* SMTP */
#endif /* ! lint */

#include <sysexits.h>

#if SMTP


static void	datatimeout __P((void));
static void	esmtp_check __P((char *, bool, MAILER *, MCI *, ENVELOPE *));
static void	helo_options __P((char *, bool, MAILER *, MCI *, ENVELOPE *));

/*
**  USERSMTP -- run SMTP protocol from the user end.
**
**	This protocol is described in RFC821.
*/

# define REPLYTYPE(r)	((r) / 100)		/* first digit of reply code */
# define REPLYCLASS(r)	(((r) / 10) % 10)	/* second digit of reply code */
# define SMTPCLOSING	421			/* "Service Shutting Down" */

#define ENHSCN(e, d)	(e) == NULL ? (d) : newstr(e)

static char	SmtpMsgBuffer[MAXLINE];		/* buffer for commands */
static char	SmtpReplyBuffer[MAXLINE];	/* buffer for replies */
static bool	SmtpNeedIntro;		/* need "while talking" in transcript */
/*
**  SMTPINIT -- initialize SMTP.
**
**	Opens the connection and sends the initial protocol.
**
**	Parameters:
**		m -- mailer to create connection to.
**		mci -- the mailer connection info.
**		e -- the envelope.
**		onlyhelo -- send only helo command?
**
**	Returns:
**		none.
**
**	Side Effects:
**		creates connection and sends initial protocol.
*/

void
smtpinit(m, mci, e, onlyhelo)
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
	bool onlyhelo;
{
	register int r;
	register char *p;
	register char *hn;
	char *enhsc;

	enhsc = NULL;
	if (tTd(18, 1))
	{
		dprintf("smtpinit ");
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
		/* FALLTHROUGH */

	  case MCIS_OPEN:
		if (!onlyhelo)
			return;
		break;

	  case MCIS_ERROR:
	  case MCIS_QUITING:
	  case MCIS_SSD:
		/* shouldn't happen */
		smtpquit(m, mci, e);
		/* FALLTHROUGH */

	  case MCIS_CLOSED:
		syserr("451 4.4.0 smtpinit: state CLOSED");
		return;

	  case MCIS_OPENING:
		break;
	}
	if (onlyhelo)
		goto helo;

	mci->mci_state = MCIS_OPENING;

	/*
	**  Get the greeting message.
	**	This should appear spontaneously.  Give it five minutes to
	**	happen.
	*/

	SmtpPhase = mci->mci_phase = "client greeting";
	sm_setproctitle(TRUE, e, "%s %s: %s",
			qid_printname(e), CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_initial, esmtp_check, NULL);
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

helo:
	if (bitnset(M_ESMTP, m->m_flags) || bitnset(M_LMTP, m->m_flags))
		mci->mci_flags |= MCIF_ESMTP;
	hn = mci->mci_heloname ? mci->mci_heloname : MyHostName;

tryhelo:
	if (bitnset(M_LMTP, m->m_flags))
	{
		smtpmessage("LHLO %s", m, mci, hn);
		SmtpPhase = mci->mci_phase = "client LHLO";
	}
	else if (bitset(MCIF_ESMTP, mci->mci_flags))
	{
		smtpmessage("EHLO %s", m, mci, hn);
		SmtpPhase = mci->mci_phase = "client EHLO";
	}
	else
	{
		smtpmessage("HELO %s", m, mci, hn);
		SmtpPhase = mci->mci_phase = "client HELO";
	}
	sm_setproctitle(TRUE, e, "%s %s: %s", qid_printname(e),
			CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_helo, helo_options, NULL);
	if (r < 0)
		goto tempfail1;
	else if (REPLYTYPE(r) == 5)
	{
		if (bitset(MCIF_ESMTP, mci->mci_flags) &&
		    !bitnset(M_LMTP, m->m_flags))
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
	    !bitnset(M_LMTP, m->m_flags) &&
	    strcasecmp(&SmtpReplyBuffer[4], MyHostName) == 0)
	{
		syserr("553 5.3.5 %s config error: mail loops back to me (MX problem?)",
			CurHostName);
		mci_setstat(mci, EX_CONFIG, "5.3.5",
			    "553 5.3.5 system config error");
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
		r = reply(m, mci, e, TimeOuts.to_miscshort, NULL, &enhsc);
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
	mci_setstat(mci, EX_TEMPFAIL, ENHSCN(enhsc, "4.4.2"), NULL);
	if (mci->mci_state != MCIS_CLOSED)
		smtpquit(m, mci, e);
	return;

  tempfail2:
	if (mci->mci_errno == 0)
		mci->mci_errno = errno;
	/* XXX should use code from other end iff ENHANCEDSTATUSCODES */
	mci_setstat(mci, EX_TEMPFAIL, ENHSCN(enhsc, "4.5.0"),
		    SmtpReplyBuffer);
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

static void
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
# if SASL
/*
**  STR_UNION -- create the union of two lists
**
**	Parameters:
**		s1, s2 -- lists of items (separated by single blanks).
**
**	Returns:
**		the union of both lists.
*/

static char *
str_union(s1, s2)
	char *s1, *s2;
{
	char *hr, *h1, *h, *res;
	int l1, l2, rl;

	if (s1 == NULL || *s1 == '\0')
		return s2;
	if (s2 == NULL || *s2 == '\0')
		return s1;
	l1 = strlen(s1);
	l2 = strlen(s2);
	rl = l1 + l2;
	res = (char *)xalloc(rl + 2);
	(void) strlcpy(res, s1, rl);
	hr = res + l1;
	h1 = s2;
	h = s2;

	/* walk through s2 */
	while (h != NULL && *h1 != '\0')
	{
		/* is there something after the current word? */
		if ((h = strchr(h1, ' ')) != NULL)
			*h = '\0';
		l1 = strlen(h1);

		/* does the current word appear in s1 ? */
		if (iteminlist(h1, s1, " ") == NULL)
		{
			/* add space as delimiter */
			*hr++ = ' ';

			/* copy the item */
			memcpy(hr, h1, l1);

			/* advance pointer in result list */
			hr += l1;
			*hr = '\0';
		}
		if (h != NULL)
		{
			/* there are more items */
			*h = ' ';
			h1 = h + 1;
		}
	}
	return res;
}
# endif /* SASL */
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

static void
helo_options(line, firstline, m, mci, e)
	char *line;
	bool firstline;
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	register char *p;

	if (firstline)
	{
# if SASL
		if (mci->mci_saslcap != NULL)
			sm_free(mci->mci_saslcap);
		mci->mci_saslcap = NULL;
# endif /* SASL */
		return;
	}

	if (strlen(line) < (SIZE_T) 5)
		return;
	line += 4;
	p = strpbrk(line, " =");
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
	else if (strcasecmp(line, "enhancedstatuscodes") == 0)
		mci->mci_flags |= MCIF_ENHSTAT;
# if STARTTLS
	else if (strcasecmp(line, "starttls") == 0)
		mci->mci_flags |= MCIF_TLS;
# endif /* STARTTLS */
# if SASL
	else if (strcasecmp(line, "auth") == 0)
	{
		if (p != NULL && *p != '\0')
		{
			if (mci->mci_saslcap != NULL)
			{
				char *h;

				/*
				**  create the union with previous auth
				**  offerings because we recognize "auth "
				**  and "auth=" (old format).
				*/
				h = mci->mci_saslcap;
				mci->mci_saslcap = str_union(h, p);
				if (h != mci->mci_saslcap)
					sm_free(h);
				mci->mci_flags |= MCIF_AUTH;
			}
			else
			{
				int l;

				l = strlen(p) + 1;
				mci->mci_saslcap = (char *)xalloc(l);
				(void) strlcpy(mci->mci_saslcap, p, l);
				mci->mci_flags |= MCIF_AUTH;
			}
		}
	}
# endif /* SASL */
}
# if SASL

/*
**  GETSASLDATA -- process the challenges from the SASL protocol
**
**	This gets the relevant sasl response data out of the reply
**	from the server
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
getsasldata(line, firstline, m, mci, e)
	char *line;
	bool firstline;
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	int len;
	char *out;
	int result;

	/* if not a continue we don't care about it */
	if ((strlen(line) <= 4) ||
	    (line[0] != '3') ||
	    (line[1] != '3') ||
	    (line[2] != '4'))
	{
		mci->mci_sasl_string = NULL;
		return;
	}

	/* forget about "334 " */
	line += 4;
	len = strlen(line);

	out = xalloc(len + 1);
	result = sasl_decode64(line, len, out, (u_int *)&len);
	if (result != SASL_OK)
	{
		len = 0;
		*out = '\0';
	}
	if (mci->mci_sasl_string != NULL)
	{
		if (mci->mci_sasl_string_len <= len)
		{
			sm_free(mci->mci_sasl_string);
			mci->mci_sasl_string = xalloc(len + 1);
		}
	}
	else
		mci->mci_sasl_string = xalloc(len + 1);
	/* XXX this is probably leaked */
	memcpy(mci->mci_sasl_string, out, len);
	mci->mci_sasl_string[len] = '\0';
	mci->mci_sasl_string_len = len;
	sm_free(out);
	return;
}

/*
**  READAUTH -- read auth value from a file
**
**	Parameters:
**		l -- line to define.
**		filename -- name of file to read.
**		safe -- if set, this is a safe read.
**
**	Returns:
**		line from file
**
**	Side Effects:
**		overwrites local static buffer. The caller should copy
**		the result.
**
*/

/* lines in authinfo file */
# define SASL_USER	1
# define SASL_AUTHID	2
# define SASL_PASSWORD	3
# define SASL_DEFREALM	4
# define SASL_MECH	5

static char *sasl_info_name[] =
{
	"",
	"user id",
	"authorization id",
	"password",
	"realm",
	"mechanism"
};

static char *
readauth(l, filename, safe)
	int l;
	char *filename;
	bool safe;
{
	FILE *f;
	long sff;
	pid_t pid;
	int lc;
	static char buf[MAXLINE];

	if (filename == NULL || filename[0] == '\0')
		return "";
#if !_FFR_ALLOW_SASLINFO
	/*
	**  make sure we don't use a program that is not
	**  accesible to the user who specified a different authinfo file.
	**  However, currently we don't pass this info (authinfo file
	**  specified by user) around, so we just turn off program access.
	*/
	if (filename[0] == '|')
	{
		auto int fd;
		int i;
		char *p;
		char *argv[MAXPV + 1];

		i = 0;
		for (p = strtok(&filename[1], " \t"); p != NULL;
		     p = strtok(NULL, " \t"))
		{
			if (i >= MAXPV)
				break;
			argv[i++] = p;
		}
		argv[i] = NULL;
		pid = prog_open(argv, &fd, CurEnv);
		if (pid < 0)
			f = NULL;
		else
			f = fdopen(fd, "r");
	}
	else
#endif /* !_FFR_ALLOW_SASLINFO */
	{
		pid = -1;
		sff = SFF_REGONLY | SFF_SAFEDIRPATH | SFF_NOWLINK
		      | SFF_NOGWFILES | SFF_NOWWFILES | SFF_NORFILES;
		if (DontLockReadFiles)
			sff |= SFF_NOLOCK;
#if _FFR_ALLOW_SASLINFO
		/*
		**  XXX: make sure we don't read or open files that are not
		**  accesible to the user who specified a different authinfo
		**  file.
		*/
		sff |= SFF_MUSTOWN;
#else /* _FFR_ALLOW_SASLINFO */
		if (safe)
			sff |= SFF_OPENASROOT;
#endif /* _FFR_ALLOW_SASLINFO */

		f = safefopen(filename, O_RDONLY, 0, sff);
	}
	if (f == NULL)
	{
		syserr("readauth: cannot open %s", filename);
		return "";
	}

	lc = 0;
	while (lc < l && fgets(buf, sizeof buf, f) != NULL)
	{
		if (buf[0] != '#')
			lc++;
	}

	(void) fclose(f);
	if (pid > 0)
		(void) waitfor(pid);
	if (lc < l)
	{
		if (LogLevel >= 9)
			sm_syslog(LOG_WARNING, NOQID, "SASL: error: can't read %s from %s",
			  sasl_info_name[l], filename);
		return "";
	}
	lc = strlen(buf) - 1;
	if (lc >= 0)
		buf[lc] = '\0';
	if (tTd(95, 6))
		dprintf("readauth(%s, %d) = '%s'\n", filename, l, buf);
	return buf;
}

#  ifndef __attribute__
#   define __attribute__(x)
#  endif /* ! __attribute__ */

static int getsimple	__P((void *, int, const char **, unsigned *));
static int getsecret	__P((sasl_conn_t *, void *, int, sasl_secret_t **));
static int saslgetrealm	__P((void *, int, const char **, const char **));

static sasl_callback_t callbacks[] =
{
	{	SASL_CB_GETREALM,	&saslgetrealm,	NULL	},
# define CB_GETREALM_IDX	0
	{	SASL_CB_PASS,		&getsecret,	NULL	},
# define CB_PASS_IDX	1
	{	SASL_CB_USER,		&getsimple,	NULL	},
# define CB_USER_IDX	2
	{	SASL_CB_AUTHNAME,	&getsimple,	NULL	},
# define CB_AUTHNAME_IDX	3
	{	SASL_CB_VERIFYFILE,	&safesaslfile,	NULL	},
	{	SASL_CB_LIST_END,	NULL,		NULL	}
};

/*
**  GETSIMPLE -- callback to get userid or authid
**
**	Parameters:
**		context -- unused
**		id -- what to do
**		result -- (pointer to) result
**		len -- (pointer to) length of result
**
**	Returns:
**		OK/failure values
*/

static int
getsimple(context, id, result, len)
	void *context __attribute__((unused));
	int id;
	const char **result;
	unsigned *len;
{
	char *h;
#  if SASL > 10509
	int addrealm;
	static int addedrealm = FALSE;
#  endif /* SASL > 10509 */
	static char *user = NULL;
	static char *authid = NULL;

	if (result == NULL)
		return SASL_BADPARAM;

	switch (id)
	{
	  case SASL_CB_USER:
		if (user == NULL)
		{
			h = readauth(SASL_USER, SASLInfo, TRUE);
			user = newstr(h);
		}
		*result = user;
		if (tTd(95, 5))
			dprintf("AUTH username '%s'\n", *result);
		if (len != NULL)
			*len = user ? strlen(user) : 0;
		break;

	  case SASL_CB_AUTHNAME:
#  if SASL > 10509
		/* XXX maybe other mechanisms too?! */
		addrealm = context != NULL &&
			   strcasecmp(context, "CRAM-MD5") == 0;
		if (addedrealm != addrealm && authid != NULL)
		{
#  if SASL > 10522
			/*
			**  digest-md5 prior to 1.5.23 doesn't copy the
			**  value it gets from the callback, but free()s
			**  it later on
			**  workaround: don't free() it here
			**  this can cause a memory leak!
			*/

			sm_free(authid);
#  endif /* SASL > 10522 */
			authid = NULL;
			addedrealm = addrealm;
		}
#  endif /* SASL > 10509 */
		if (authid == NULL)
		{
			h = readauth(SASL_AUTHID, SASLInfo, TRUE);
#  if SASL > 10509
			if (addrealm && strchr(h, '@') == NULL)
			{
				size_t l;
				char *realm;

				realm = callbacks[CB_GETREALM_IDX].context;
				l = strlen(h) + strlen(realm) + 2;
				authid = xalloc(l);
				snprintf(authid, l, "%s@%s", h, realm);
			}
			else
#  endif /* SASL > 10509 */
				authid = newstr(h);
		}
		*result = authid;
		if (tTd(95, 5))
			dprintf("AUTH authid '%s'\n", *result);
		if (len != NULL)
			*len = authid ? strlen(authid) : 0;
		break;

	  case SASL_CB_LANGUAGE:
		*result = NULL;
		if (len != NULL)
			*len = 0;
		break;

	  default:
		return SASL_BADPARAM;
	}
	return SASL_OK;
}

/*
**  GETSECRET -- callback to get password
**
**	Parameters:
**		conn -- connection information
**		context -- unused
**		id -- what to do
**		psecret -- (pointer to) result
**
**	Returns:
**		OK/failure values
*/

static int
getsecret(conn, context, id, psecret)
	sasl_conn_t *conn;
	void *context __attribute__((unused));
	int id;
	sasl_secret_t **psecret;
{
	char *h;
	int len;
	static char *authpass = NULL;

	if (conn == NULL || psecret == NULL || id != SASL_CB_PASS)
		return SASL_BADPARAM;

	if (authpass == NULL)
	{
		h = readauth(SASL_PASSWORD, SASLInfo, TRUE);
		authpass = newstr(h);
	}
	len = strlen(authpass);
	*psecret = (sasl_secret_t *) xalloc(sizeof(sasl_secret_t) + len + 1);
	(void) strlcpy((*psecret)->data, authpass, len + 1);
	(*psecret)->len = len;
	return SASL_OK;
}

/*
**  SAFESASLFILE -- callback for sasl: is file safe?
**
**	Parameters:
**		context -- pointer to context between invocations (unused)
**		file -- name of file to check
**		type -- type of file to check
**
**	Returns:
**		SASL_OK: file can be used
**		SASL_CONTINUE: don't use file
**		SASL_FAIL: failure (not used here)
**
*/
int
# if SASL > 10515
safesaslfile(context, file, type)
# else /* SASL > 10515 */
safesaslfile(context, file)
# endif /* SASL > 10515 */
	void *context;
	char *file;
# if SASL > 10515
	int type;
# endif /* SASL > 10515 */
{
	long sff;
	int r;
	char *p;

	if (file == NULL || *file == '\0')
		return SASL_OK;

	sff = SFF_SAFEDIRPATH|SFF_NOWLINK|SFF_NOGWFILES|SFF_NOWWFILES|SFF_ROOTOK;
	if ((p = strrchr(file, '/')) == NULL)
		p = file;
	else
		++p;

# if SASL <= 10515
	/* everything beside libs and .conf files must not be readable */
	r = strlen(p);
	if ((r <= 3 || strncmp(p, "lib", 3) != 0) &&
	    (r <= 5 || strncmp(p + r - 5, ".conf", 5) != 0)
#  if _FFR_UNSAFE_SASL
	    && !bitnset(DBS_GROUPREADABLESASLFILE, DontBlameSendmail)
#  endif /* _FFR_UNSAFE_SASL */
	   )
		sff |= SFF_NORFILES;
# else /* SASL > 10515 */
	/* files containing passwords should be not readable */
	if (type == SASL_VRFY_PASSWD)
	{
#  if _FFR_UNSAFE_SASL
		if (bitnset(DBS_GROUPREADABLESASLFILE, DontBlameSendmail))
			sff |= SFF_NOWRFILES;
		else
#  endif /* _FFR_UNSAFE_SASL */
			sff |= SFF_NORFILES;
	}
# endif /* SASL <= 10515 */

	p = file;
	if ((r = safefile(p, RunAsUid, RunAsGid, RunAsUserName, sff,
			  S_IRUSR, NULL)) == 0)
		return SASL_OK;
	if (LogLevel >= 11 || (r != ENOENT && LogLevel >= 9))
		sm_syslog(LOG_WARNING, NOQID, "error: safesasl(%s) failed: %s",
			  p, errstring(r));
	return SASL_CONTINUE;
}

/*
**  SASLGETREALM -- return the realm for SASL
**
**	return the realm for the client
**
**	Parameters:
**		context -- context shared between invocations
**			here: realm to return
**		availrealms -- list of available realms
**			{realm, realm, ...}
**		result -- pointer to result
**
**	Returns:
**		failure/success
*/
static int
saslgetrealm(context, id, availrealms, result)
	void *context;
	int id;
	const char **availrealms;
	const char **result;
{
	if (LogLevel > 12)
		sm_syslog(LOG_INFO, NOQID, "saslgetrealm: realm %s available realms %s",
			  context == NULL ? "<No Context>" : (char *) context,
			  (availrealms == NULL || *availrealms == NULL) ? "<No Realms>" : *availrealms);
	if (context == NULL)
		return SASL_FAIL;

	/* check whether context is in list? */
	if (availrealms != NULL && *availrealms != NULL)
	{
		if (iteminlist(context, (char *)(*availrealms + 1), " ,}") ==
		    NULL)
		{
			if (LogLevel > 8)
				sm_syslog(LOG_ERR, NOQID,
					  "saslgetrealm: realm %s not in list %s",
					  context, *availrealms);
			return SASL_FAIL;
		}
	}
	*result = (char *)context;
	return SASL_OK;
}
/*
**  ITEMINLIST -- does item appear in list?
**
**	Check whether item appears in list (which must be separated by a
**	character in delim) as a "word", i.e. it must appear at the begin
**	of the list or after a space, and it must end with a space or the
**	end of the list.
**
**	Parameters:
**		item -- item to search.
**		list -- list of items.
**		delim -- list of delimiters.
**
**	Returns:
**		pointer to occurrence (NULL if not found).
*/

char *
iteminlist(item, list, delim)
	char *item;
	char *list;
	char *delim;
{
	char *s;
	int len;

	if (list == NULL || *list == '\0')
		return NULL;
	if (item == NULL || *item == '\0')
		return NULL;
	s = list;
	len = strlen(item);
	while (s != NULL && *s != '\0')
	{
		if (strncasecmp(s, item, len) == 0 &&
		    (s[len] == '\0' || strchr(delim, s[len]) != NULL))
			return s;
		s = strpbrk(s, delim);
		if (s != NULL)
			while (*++s == ' ')
				continue;
	}
	return NULL;
}
/*
**  REMOVEMECH -- remove item [rem] from list [list]
**
**	Parameters:
**		rem -- item to remove
**		list -- list of items
**
**	Returns:
**		pointer to new list (NULL in case of error).
*/

char *
removemech(rem, list)
	char *rem;
	char *list;
{
	char *ret;
	char *needle;
	int len;

	if (list == NULL)
		return NULL;
	if (rem == NULL || *rem == '\0')
	{
		/* take out what? */
		return NULL;
	}

	/* find the item in the list */
	if ((needle = iteminlist(rem, list, " ")) == NULL)
	{
		/* not in there: return original */
		return list;
	}

	/* length of string without rem */
	len = strlen(list) - strlen(rem);
	if (len == 0)
	{
		ret = xalloc(1);  /* XXX leaked */
		*ret = '\0';
		return ret;
	}
	ret = xalloc(len);  /* XXX leaked */
	memset(ret, '\0', len);

	/* copy from start to removed item */
	memcpy(ret, list, needle - list);

	/* length of rest of string past removed item */
	len = strlen(needle) - strlen(rem) - 1;
	if (len > 0)
	{
		/* not last item -- copy into string */
		memcpy(ret + (needle - list),
		       list + (needle - list) + strlen(rem) + 1,
		       len);
	}
	else
		ret[(needle - list) - 1] = '\0';
	return ret;
}
/*
**  INTERSECT -- create the intersection between two lists
**
**	Parameters:
**		s1, s2 -- lists of items (separated by single blanks).
**
**	Returns:
**		the intersection of both lists.
*/

char *
intersect(s1, s2)
	char *s1, *s2;
{
	char *hr, *h1, *h, *res;
	int l1, l2, rl;

	if (s1 == NULL || s2 == NULL)	/* NULL string(s) -> NULL result */
		return NULL;
	l1 = strlen(s1);
	l2 = strlen(s2);
	rl = min(l1, l2);
	res = (char *)xalloc(rl + 1);
	*res = '\0';
	if (rl == 0)	/* at least one string empty? */
		return res;
	hr = res;
	h1 = s1;
	h = s1;

	/* walk through s1 */
	while (h != NULL && *h1 != '\0')
	{
		/* is there something after the current word? */
		if ((h = strchr(h1, ' ')) != NULL)
			*h = '\0';
		l1 = strlen(h1);

		/* does the current word appear in s2 ? */
		if (iteminlist(h1, s2, " ") != NULL)
		{
			/* add a blank if not first item */
			if (hr != res)
				*hr++ = ' ';

			/* copy the item */
			memcpy(hr, h1, l1);

			/* advance pointer in result list */
			hr += l1;
			*hr = '\0';
		}
		if (h != NULL)
		{
			/* there are more items */
			*h = ' ';
			h1 = h + 1;
		}
	}
	return res;
}
/*
**  ATTEMPTAUTH -- try to AUTHenticate using one mechanism
**
**	Parameters:
**		m -- the mailer.
**		mci -- the mailer connection structure.
**		e -- the envelope (including the sender to specify).
**		mechused - filled in with mechanism used
**
**	Returns:
**		EX_OK/EX_TEMPFAIL
*/

int
attemptauth(m, mci, e, mechused)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
	char **mechused;
{
	int saslresult, smtpresult;
	sasl_external_properties_t ssf;
	sasl_interact_t *client_interact = NULL;
	char *out;
	unsigned int outlen;
	static char *mechusing;
	sasl_security_properties_t ssp;
	char in64[MAXOUTLEN];
# if NETINET
	extern SOCKADDR CurHostAddr;
# endif /* NETINET */

	*mechused = NULL;
	if (mci->mci_conn != NULL)
	{
		sasl_dispose(&(mci->mci_conn));

		/* just in case, sasl_dispose() should take care of it */
		mci->mci_conn = NULL;
	}

	/* make a new client sasl connection */
	saslresult = sasl_client_new(bitnset(M_LMTP, m->m_flags) ? "lmtp"
								 : "smtp",
				     CurHostName, NULL, 0, &mci->mci_conn);

	/* set properties */
	(void) memset(&ssp, '\0', sizeof ssp);
#  if SFIO
	/* XXX should these be options settable via .cf ? */
	/* ssp.min_ssf = 0; is default due to memset() */
	{
		ssp.max_ssf = INT_MAX;
		ssp.maxbufsize = MAXOUTLEN;
#   if 0
		ssp.security_flags = SASL_SEC_NOPLAINTEXT;
#   endif /* 0 */
	}
#  endif /* SFIO */
	saslresult = sasl_setprop(mci->mci_conn, SASL_SEC_PROPS, &ssp);
	if (saslresult != SASL_OK)
		return EX_TEMPFAIL;

	/* external security strength factor, authentication id */
	ssf.ssf = 0;
	ssf.auth_id = NULL;
# if _FFR_EXT_MECH
	out = macvalue(macid("{cert_subject}", NULL), e);
	if (out != NULL && *out != '\0')
		ssf.auth_id = out;
	out = macvalue(macid("{cipher_bits}", NULL), e);
	if (out != NULL && *out != '\0')
		ssf.ssf = atoi(out);
# endif /* _FFR_EXT_MECH */
	saslresult = sasl_setprop(mci->mci_conn, SASL_SSF_EXTERNAL, &ssf);
	if (saslresult != SASL_OK)
		return EX_TEMPFAIL;

# if NETINET
	/* set local/remote ipv4 addresses */
	if (mci->mci_out != NULL && CurHostAddr.sa.sa_family == AF_INET)
	{
		SOCKADDR_LEN_T addrsize;
		struct sockaddr_in saddr_l;

		if (sasl_setprop(mci->mci_conn, SASL_IP_REMOTE,
				 (struct sockaddr_in *) &CurHostAddr)
		    != SASL_OK)
			return EX_TEMPFAIL;
		addrsize = sizeof(struct sockaddr_in);
		if (getsockname(fileno(mci->mci_out),
				(struct sockaddr *) &saddr_l, &addrsize) == 0)
		{
			if (sasl_setprop(mci->mci_conn, SASL_IP_LOCAL,
					 &saddr_l) != SASL_OK)
				return EX_TEMPFAIL;
		}
	}
# endif /* NETINET */

	/* start client side of sasl */
	saslresult = sasl_client_start(mci->mci_conn, mci->mci_saslcap,
				       NULL, &client_interact,
				       &out, &outlen,
				       (const char **)&mechusing);
	callbacks[CB_AUTHNAME_IDX].context = mechusing;

	if (saslresult != SASL_OK && saslresult != SASL_CONTINUE)
	{
#  if SFIO
		if (saslresult == SASL_NOMECH && LogLevel > 8)
		{
			sm_syslog(LOG_NOTICE, e->e_id,
				  "available AUTH mechanisms do not fulfill requirements");
		}
#  endif /* SFIO */
		return EX_TEMPFAIL;
	}

	*mechused = mechusing;

	/* send the info across the wire */
	if (outlen > 0)
	{
		saslresult = sasl_encode64(out, outlen, in64, MAXOUTLEN, NULL);
		if (saslresult != SASL_OK) /* internal error */
		{
			if (LogLevel > 8)
				sm_syslog(LOG_ERR, e->e_id,
					"encode64 for AUTH failed");
			return EX_TEMPFAIL;
		}
		smtpmessage("AUTH %s %s", m, mci, mechusing, in64);
	}
	else
	{
		smtpmessage("AUTH %s", m, mci, mechusing);
	}

	/* get the reply */
	smtpresult = reply(m, mci, e, TimeOuts.to_datafinal, getsasldata, NULL);
	/* which timeout? XXX */

	for (;;)
	{
		/* check return code from server */
		if (smtpresult == 235)
		{
			define(macid("{auth_type}", NULL),
			       newstr(mechusing), e);
#  if !SFIO
			if (LogLevel > 9)
				sm_syslog(LOG_INFO, NOQID,
					  "SASL: outgoing connection to %.64s: mech=%.16s",
					  mci->mci_host, mechusing);
#  endif /* !SFIO */
			return EX_OK;
		}
		if (smtpresult == -1)
			return EX_IOERR;
		if (smtpresult != 334)
			return EX_TEMPFAIL;

		saslresult = sasl_client_step(mci->mci_conn,
					      mci->mci_sasl_string,
					      mci->mci_sasl_string_len,
					      &client_interact,
					      &out, &outlen);

		if (saslresult != SASL_OK && saslresult != SASL_CONTINUE)
		{
			if (tTd(95, 5))
				dprintf("AUTH FAIL: %s (%d)\n",
					sasl_errstring(saslresult, NULL, NULL),
					saslresult);

			/* fail deliberately, see RFC 2254 4. */
			smtpmessage("*", m, mci);

			/*
			**  but we should only fail for this authentication
			**  mechanism; how to do that?
			*/

			smtpresult = reply(m, mci, e, TimeOuts.to_datafinal,
					   getsasldata, NULL);
			return EX_TEMPFAIL;
		}

		if (outlen > 0)
		{
			saslresult = sasl_encode64(out, outlen, in64,
						   MAXOUTLEN, NULL);
			if (saslresult != SASL_OK)
			{
				/* give an error reply to the other side! */
				smtpmessage("*", m, mci);
				return EX_TEMPFAIL;
			}
		}
		else
			in64[0] = '\0';
		smtpmessage("%s", m, mci, in64);
		smtpresult = reply(m, mci, e, TimeOuts.to_datafinal,
				   getsasldata, NULL);
		/* which timeout? XXX */
	}
	/* NOTREACHED */
}

/*
**  SMTPAUTH -- try to AUTHenticate
**
**	This will try mechanisms in the order the sasl library decided until:
**	- there are no more mechanisms
**	- a mechanism succeeds
**	- the sasl library fails initializing
**
**	Parameters:
**		m -- the mailer.
**		mci -- the mailer connection info.
**		e -- the envelope.
**
**	Returns:
**		EX_OK/EX_TEMPFAIL
*/

int
smtpauth(m, mci, e)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	int result;
	char *mechused;
	char *h;
	static char *defrealm = NULL;
	static char *mechs = NULL;

	mci->mci_sasl_auth = FALSE;
	if (defrealm == NULL)
	{
		h = readauth(SASL_DEFREALM, SASLInfo, TRUE);
		if (h != NULL && *h != '\0')
			defrealm = newstr(h);
	}
	if (defrealm == NULL || *defrealm == '\0')
		defrealm = newstr(macvalue('j', CurEnv));
	callbacks[CB_GETREALM_IDX].context = defrealm;

# if _FFR_DEFAUTHINFO_MECHS
	if (mechs == NULL)
	{
		h = readauth(SASL_MECH, SASLInfo, TRUE);
		if (h != NULL && *h != '\0')
			mechs = newstr(h);
	}
# endif /* _FFR_DEFAUTHINFO_MECHS */
	if (mechs == NULL || *mechs == '\0')
		mechs = AuthMechanisms;
	mci->mci_saslcap = intersect(mechs, mci->mci_saslcap);

	/* initialize sasl client library */
	result = sasl_client_init(callbacks);
	if (result != SASL_OK)
		return EX_TEMPFAIL;
	do
	{
		result = attemptauth(m, mci, e, &mechused);
		if (result == EX_OK)
			mci->mci_sasl_auth = TRUE;
		else if (result == EX_TEMPFAIL)
		{
			mci->mci_saslcap = removemech(mechused,
						      mci->mci_saslcap);
			if (mci->mci_saslcap == NULL ||
			    *(mci->mci_saslcap) == '\0')
				return EX_TEMPFAIL;
		}
		else	/* all others for now */
			return EX_TEMPFAIL;
	} while (result != EX_OK);
	return result;
}
# endif /* SASL */

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
	char *bufp;
	char *bodytype;
	char buf[MAXNAME + 1];
	char optbuf[MAXLINE];
	char *enhsc;

	if (tTd(18, 2))
		dprintf("smtpmailfrom: CurHost=%s\n", CurHostName);
	enhsc = NULL;

	/* set up appropriate options to include */
	if (bitset(MCIF_SIZE, mci->mci_flags) && e->e_msgsize > 0)
	{
		snprintf(optbuf, sizeof optbuf, " SIZE=%ld", e->e_msgsize);
		bufp = &optbuf[strlen(optbuf)];
	}
	else
	{
		optbuf[0] = '\0';
		bufp = optbuf;
	}

	bodytype = e->e_bodytype;
	if (bitset(MCIF_8BITMIME, mci->mci_flags))
	{
		if (bodytype == NULL &&
		    bitset(MM_MIME8BIT, MimeMode) &&
		    bitset(EF_HAS8BIT, e->e_flags) &&
		    !bitset(EF_DONT_MIME, e->e_flags) &&
		    !bitnset(M_8BITS, m->m_flags))
			bodytype = "8BITMIME";
		if (bodytype != NULL &&
		    SPACELEFT(optbuf, bufp) > strlen(bodytype) + 7)
		{
			snprintf(bufp, SPACELEFT(optbuf, bufp),
				 " BODY=%s", bodytype);
			bufp += strlen(bufp);
		}
	}
	else if (bitnset(M_8BITS, m->m_flags) ||
		 !bitset(EF_HAS8BIT, e->e_flags) ||
		 bitset(MCIF_8BITOK, mci->mci_flags))
	{
		/* EMPTY */
		/* just pass it through */
	}
# if MIME8TO7
	else if (bitset(MM_CVTMIME, MimeMode) &&
		 !bitset(EF_DONT_MIME, e->e_flags) &&
		 (!bitset(MM_PASS8BIT, MimeMode) ||
		  bitset(EF_IS_MIME, e->e_flags)))
	{
		/* must convert from 8bit MIME format to 7bit encoded */
		mci->mci_flags |= MCIF_CVT8TO7;
	}
# endif /* MIME8TO7 */
	else if (!bitset(MM_PASS8BIT, MimeMode))
	{
		/* cannot just send a 8-bit version */
		extern char MsgBuf[];

		usrerrenh("5.6.3", "%s does not support 8BITMIME", CurHostName);
		mci_setstat(mci, EX_NOTSTICKY, "5.6.3", MsgBuf);
		return EX_DATAERR;
	}

	if (bitset(MCIF_DSN, mci->mci_flags))
	{
		if (e->e_envid != NULL &&
		    SPACELEFT(optbuf, bufp) > strlen(e->e_envid) + 7)
		{
			snprintf(bufp, SPACELEFT(optbuf, bufp),
				 " ENVID=%s", e->e_envid);
			bufp += strlen(bufp);
		}

		/* RET= parameter */
		if (bitset(EF_RET_PARAM, e->e_flags) &&
		    SPACELEFT(optbuf, bufp) > 9)
		{
			snprintf(bufp, SPACELEFT(optbuf, bufp),
				 " RET=%s",
				 bitset(EF_NO_BODY_RETN, e->e_flags) ?
					"HDRS" : "FULL");
			bufp += strlen(bufp);
		}
	}

	if (bitset(MCIF_AUTH, mci->mci_flags) && e->e_auth_param != NULL &&
	    SPACELEFT(optbuf, bufp) > strlen(e->e_auth_param) + 7
# if SASL
	     && (!bitset(SASL_AUTH_AUTH, SASLOpts) || mci->mci_sasl_auth)
# endif /* SASL */
	    )
	{
		snprintf(bufp, SPACELEFT(optbuf, bufp),
			 " AUTH=%s", e->e_auth_param);
		bufp += strlen(bufp);
	}

	/*
	**  Send the MAIL command.
	**	Designates the sender.
	*/

	mci->mci_state = MCIS_ACTIVE;

	if (bitset(EF_RESPONSE, e->e_flags) &&
	    !bitnset(M_NO_NULL_FROM, m->m_flags))
		buf[0] = '\0';
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
	sm_setproctitle(TRUE, e, "%s %s: %s", qid_printname(e),
			CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_mail, NULL, &enhsc);
	if (r < 0)
	{
		/* communications failure */
		mci->mci_errno = errno;
		mci_setstat(mci, EX_TEMPFAIL, "4.4.2", NULL);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	else if (r == SMTPCLOSING)
	{
		/* service shutting down */
		mci_setstat(mci, EX_TEMPFAIL, ENHSCN(enhsc, "4.5.0"),
			    SmtpReplyBuffer);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	else if (REPLYTYPE(r) == 4)
	{
		mci_setstat(mci, EX_NOTSTICKY, ENHSCN(enhsc, smtptodsn(r)),
			    SmtpReplyBuffer);
		return EX_TEMPFAIL;
	}
	else if (REPLYTYPE(r) == 2)
	{
		return EX_OK;
	}
	else if (r == 501)
	{
		/* syntax error in arguments */
		mci_setstat(mci, EX_NOTSTICKY, ENHSCN(enhsc, "5.5.2"),
			    SmtpReplyBuffer);
		return EX_DATAERR;
	}
	else if (r == 553)
	{
		/* mailbox name not allowed */
		mci_setstat(mci, EX_NOTSTICKY, ENHSCN(enhsc, "5.1.3"),
			    SmtpReplyBuffer);
		return EX_DATAERR;
	}
	else if (r == 552)
	{
		/* exceeded storage allocation */
		mci_setstat(mci, EX_NOTSTICKY, ENHSCN(enhsc, "5.3.4"),
			    SmtpReplyBuffer);
		if (bitset(MCIF_SIZE, mci->mci_flags))
			e->e_flags |= EF_NO_BODY_RETN;
		return EX_UNAVAILABLE;
	}
	else if (REPLYTYPE(r) == 5)
	{
		/* unknown error */
		mci_setstat(mci, EX_NOTSTICKY, ENHSCN(enhsc, "5.0.0"),
			    SmtpReplyBuffer);
		return EX_UNAVAILABLE;
	}

	if (LogLevel > 1)
	{
		sm_syslog(LOG_CRIT, e->e_id,
			  "%.100s: SMTP MAIL protocol error: %s",
			  CurHostName,
			  shortenstring(SmtpReplyBuffer, 403));
	}

	/* protocol error -- close up */
	mci_setstat(mci, EX_PROTOCOL, ENHSCN(enhsc, "5.5.1"),
		    SmtpReplyBuffer);
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
	char *bufp;
	char optbuf[MAXLINE];
	char *enhsc;

	enhsc = NULL;
	optbuf[0] = '\0';
	bufp = optbuf;

	/*
	**  warning: in the following it is assumed that the free space
	**  in bufp is sizeof optbuf
	*/
	if (bitset(MCIF_DSN, mci->mci_flags))
	{
		/* NOTIFY= parameter */
		if (bitset(QHASNOTIFY, to->q_flags) &&
		    bitset(QPRIMARY, to->q_flags) &&
		    !bitnset(M_LOCALMAILER, m->m_flags))
		{
			bool firstone = TRUE;

			(void) strlcat(bufp, " NOTIFY=", sizeof optbuf);
			if (bitset(QPINGONSUCCESS, to->q_flags))
			{
				(void) strlcat(bufp, "SUCCESS", sizeof optbuf);
				firstone = FALSE;
			}
			if (bitset(QPINGONFAILURE, to->q_flags))
			{
				if (!firstone)
					(void) strlcat(bufp, ",",
						       sizeof optbuf);
				(void) strlcat(bufp, "FAILURE", sizeof optbuf);
				firstone = FALSE;
			}
			if (bitset(QPINGONDELAY, to->q_flags))
			{
				if (!firstone)
					(void) strlcat(bufp, ",",
						       sizeof optbuf);
				(void) strlcat(bufp, "DELAY", sizeof optbuf);
				firstone = FALSE;
			}
			if (firstone)
				(void) strlcat(bufp, "NEVER", sizeof optbuf);
			bufp += strlen(bufp);
		}

		/* ORCPT= parameter */
		if (to->q_orcpt != NULL &&
		    SPACELEFT(optbuf, bufp) > strlen(to->q_orcpt) + 7)
		{
			snprintf(bufp, SPACELEFT(optbuf, bufp),
				 " ORCPT=%s", to->q_orcpt);
			bufp += strlen(bufp);
		}
	}

	smtpmessage("RCPT To:<%s>%s", m, mci, to->q_user, optbuf);

	SmtpPhase = mci->mci_phase = "client RCPT";
	sm_setproctitle(TRUE, e, "%s %s: %s", qid_printname(e),
			CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_rcpt, NULL, &enhsc);
	to->q_rstatus = newstr(SmtpReplyBuffer);
	to->q_status = ENHSCN(enhsc, smtptodsn(r));
	if (!bitnset(M_LMTP, m->m_flags))
		to->q_statmta = mci->mci_host;
	if (r < 0 || REPLYTYPE(r) == 4)
		return EX_TEMPFAIL;
	else if (REPLYTYPE(r) == 2)
		return EX_OK;
	else if (r == 550)
	{
		to->q_status = ENHSCN(enhsc, "5.1.1");
		return EX_NOUSER;
	}
	else if (r == 551)
	{
		to->q_status = ENHSCN(enhsc, "5.1.6");
		return EX_NOUSER;
	}
	else if (r == 553)
	{
		to->q_status = ENHSCN(enhsc, "5.1.3");
		return EX_NOUSER;
	}
	else if (REPLYTYPE(r) == 5)
	{
		return EX_UNAVAILABLE;
	}

	if (LogLevel > 1)
	{
		sm_syslog(LOG_CRIT, e->e_id,
			  "%.100s: SMTP RCPT protocol error: %s",
			  CurHostName,
			  shortenstring(SmtpReplyBuffer, 403));
	}

	mci_setstat(mci, EX_PROTOCOL, ENHSCN(enhsc, "5.5.1"),
		    SmtpReplyBuffer);
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
static EVENT	*volatile DataTimeout = NULL;

int
smtpdata(m, mci, e)
	MAILER *m;
	register MCI *mci;
	register ENVELOPE *e;
{
	register int r;
	int rstat;
	int xstat;
	time_t timeout;
	char *enhsc;

	enhsc = NULL;

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
	sm_setproctitle(TRUE, e, "%s %s: %s",
			qid_printname(e), CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_datainit, NULL, &enhsc);
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
	else if (REPLYTYPE(r) != 3)
	{
		if (LogLevel > 1)
		{
			sm_syslog(LOG_CRIT, e->e_id,
				  "%.100s: SMTP DATA-1 protocol error: %s",
				  CurHostName,
				  shortenstring(SmtpReplyBuffer, 403));
		}
		smtprset(m, mci, e);
		mci_setstat(mci, EX_PROTOCOL, ENHSCN(enhsc, "5.5.1"),
			    SmtpReplyBuffer);
		return EX_PROTOCOL;
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

		/*
		**  If putbody() couldn't finish due to a timeout,
		**  rewind it here in the timeout handler.  See
		**  comments at the end of putbody() for reasoning.
		*/

		if (e->e_dfp != NULL)
			(void) bfrewind(e->e_dfp);

		errno = mci->mci_errno;
		syserr("451 4.4.1 timeout writing message to %s", CurHostName);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}

	if (tTd(18, 101))
	{
		/* simulate a DATA timeout */
		timeout = 1;
	}
	else
		timeout = DATA_PROGRESS_TIMEOUT;

	DataTimeout = setevent(timeout, datatimeout, 0);


	/*
	**  Output the actual message.
	*/

	(*e->e_puthdr)(mci, e->e_header, e, M87F_OUTER);

	if (tTd(18, 101))
	{
		/* simulate a DATA timeout */
		(void) sleep(2);
	}

	(*e->e_putbody)(mci, e, NULL);

	/*
	**  Cleanup after sending message.
	*/

	if (DataTimeout != NULL)
		clrevent(DataTimeout);

# if _FFR_CATCH_BROKEN_MTAS
	{
		fd_set readfds;
		struct timeval timeout;

		FD_ZERO(&readfds);
		FD_SET(fileno(mci->mci_in), &readfds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select(fileno(mci->mci_in) + 1, FDSET_CAST &readfds,
			   NULL, NULL, &timeout) > 0 &&
		    FD_ISSET(fileno(mci->mci_in), &readfds))
		{
			/* terminate the message */
			fprintf(mci->mci_out, ".%s", m->m_eol);
			if (TrafficLogFile != NULL)
				fprintf(TrafficLogFile, "%05d >>> .\n",
					(int) getpid());
			if (Verbose)
				nmessage(">>> .");

			mci->mci_errno = EIO;
			mci->mci_state = MCIS_ERROR;
			mci_setstat(mci, EX_PROTOCOL, "5.5.0", NULL);
			smtpquit(m, mci, e);
			return EX_PROTOCOL;
		}
	}
# endif /* _FFR_CATCH_BROKEN_MTAS */

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
	sm_setproctitle(TRUE, e, "%s %s: %s", qid_printname(e),
			CurHostName, mci->mci_phase);
	if (bitnset(M_LMTP, m->m_flags))
		return EX_OK;
	r = reply(m, mci, e, TimeOuts.to_datafinal, NULL, &enhsc);
	if (r < 0)
	{
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	mci->mci_state = MCIS_OPEN;
	xstat = EX_NOTSTICKY;
	if (r == 452)
		rstat = EX_TEMPFAIL;
	else if (REPLYTYPE(r) == 4)
		rstat = xstat = EX_TEMPFAIL;
	else if (REPLYCLASS(r) != 5)
		rstat = xstat = EX_PROTOCOL;
	else if (REPLYTYPE(r) == 2)
		rstat = xstat = EX_OK;
	else if (REPLYTYPE(r) == 5)
		rstat = EX_UNAVAILABLE;
	else
		rstat = EX_PROTOCOL;
	mci_setstat(mci, xstat, ENHSCN(enhsc, smtptodsn(r)),
		    SmtpReplyBuffer);
	if (e->e_statmsg != NULL)
		sm_free(e->e_statmsg);
	if (bitset(MCIF_ENHSTAT, mci->mci_flags) &&
	    (r = isenhsc(SmtpReplyBuffer + 4, ' ')) > 0)
		r += 5;
	else
		r = 4;
	e->e_statmsg = newstr(&SmtpReplyBuffer[r]);
	SmtpPhase = mci->mci_phase = "idle";
	sm_setproctitle(TRUE, e, "%s: %s", CurHostName, mci->mci_phase);
	if (rstat != EX_PROTOCOL)
		return rstat;
	if (LogLevel > 1)
	{
		sm_syslog(LOG_CRIT, e->e_id,
			  "%.100s: SMTP DATA-2 protocol error: %s",
			  CurHostName,
			  shortenstring(SmtpReplyBuffer, 403));
	}
	return rstat;
}


static void
datatimeout()
{
	int save_errno = errno;

	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	if (DataProgress)
	{
		time_t timeout;

		/* check back again later */
		if (tTd(18, 101))
		{
			/* simulate a DATA timeout */
			timeout = 1;
		}
		else
			timeout = DATA_PROGRESS_TIMEOUT;

		/* reset the timeout */
		DataTimeout = sigsafe_setevent(timeout, datatimeout, 0);
		DataProgress = FALSE;
	}
	else
	{
		/* event is done */
		DataTimeout = NULL;
	}

	/* if no progress was made or problem resetting event, die now */
	if (DataTimeout == NULL)
	{
		errno = ETIMEDOUT;
		longjmp(CtxDataTimeout, 1);
	}

	errno = save_errno;
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

int
smtpgetstat(m, mci, e)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	int r;
	int status;
	char *enhsc;

	enhsc = NULL;
	/* check for the results of the transaction */
	r = reply(m, mci, e, TimeOuts.to_datafinal, NULL, &enhsc);
	if (r < 0)
	{
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	if (REPLYTYPE(r) == 4)
		status = EX_TEMPFAIL;
	else if (REPLYCLASS(r) != 5)
		status = EX_PROTOCOL;
	else if (REPLYTYPE(r) == 2)
		status = EX_OK;
	else if (REPLYTYPE(r) == 5)
		status = EX_UNAVAILABLE;
	else
		status = EX_PROTOCOL;
	if (e->e_statmsg != NULL)
		sm_free(e->e_statmsg);
	if (bitset(MCIF_ENHSTAT, mci->mci_flags) &&
	    (r = isenhsc(SmtpReplyBuffer + 4, ' ')) > 0)
		r += 5;
	else
		r = 4;
	e->e_statmsg = newstr(&SmtpReplyBuffer[r]);
	mci_setstat(mci, status, ENHSCN(enhsc, smtptodsn(r)),
		    SmtpReplyBuffer);
	if (LogLevel > 1 && status == EX_PROTOCOL)
	{
		sm_syslog(LOG_CRIT, e->e_id,
			  "%.100s: SMTP DATA-3 protocol error: %s",
			  CurHostName,
			  shortenstring(SmtpReplyBuffer, 403));
	}
	return status;
}
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
	int rcode;

	CurHostName = mci->mci_host;		/* XXX UGLY XXX */
	if (CurHostName == NULL)
		CurHostName = MyHostName;

	/*
	**	Suppress errors here -- we may be processing a different
	**	job when we do the quit connection, and we don't want the
	**	new job to be penalized for something that isn't it's
	**	problem.
	*/

	SuprErrs = TRUE;

	/* send the quit message if we haven't gotten I/O error */
	if (mci->mci_state != MCIS_ERROR &&
	    mci->mci_state != MCIS_QUITING)
	{
		int origstate = mci->mci_state;

		SmtpPhase = "client QUIT";
		mci->mci_state = MCIS_QUITING;
		smtpmessage("QUIT", m, mci);
		(void) reply(m, mci, e, TimeOuts.to_quit, NULL, NULL);
		SuprErrs = oldSuprErrs;
		if (mci->mci_state == MCIS_CLOSED ||
		    origstate == MCIS_CLOSED)
			return;
	}

	/* now actually close the connection and pick up the zombie */
	rcode = endmailer(mci, e, NULL);
	if (rcode != EX_OK)
	{
		char *mailer = NULL;

		if (mci->mci_mailer != NULL &&
		    mci->mci_mailer->m_name != NULL)
			mailer = mci->mci_mailer->m_name;

		/* look for naughty mailers */
		sm_syslog(LOG_ERR, e->e_id,
			  "smtpquit: mailer%s%s exited with exit value %d",
			  mailer == NULL ? "" : " ",
			  mailer == NULL ? "" : mailer,
			  rcode);
	}

	SuprErrs = oldSuprErrs;
}
/*
**  SMTPRSET -- send a RSET (reset) command
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
**		closes the connection if there is no reply to RSET.
*/

void
smtprset(m, mci, e)
	register MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	int r;

	CurHostName = mci->mci_host;		/* XXX UGLY XXX */
	if (CurHostName == NULL)
		CurHostName = MyHostName;

	SmtpPhase = "client RSET";
	smtpmessage("RSET", m, mci);
	r = reply(m, mci, e, TimeOuts.to_rset, NULL, NULL);
	if (r < 0)
		mci->mci_state = MCIS_ERROR;
	else
	{
		/*
		**  Any response is deemed to be acceptable.
		**  The standard does not state the proper action
		**  to take when a value other than 250 is received.
		**
		**  However, if 421 is returned for the RSET, leave
		**  mci_state as MCIS_SSD (set in reply()).
		*/

		if (mci->mci_state != MCIS_SSD)
			mci->mci_state = MCIS_OPEN;
		return;
	}
	smtpquit(m, mci, e);
}
/*
**  SMTPPROBE -- check the connection state
**
**	Parameters:
**		mci -- the mailer connection information.
**
**	Returns:
**		none.
**
**	Side Effects:
**		closes the connection if there is no reply to RSET.
*/

int
smtpprobe(mci)
	register MCI *mci;
{
	int r;
	MAILER *m = mci->mci_mailer;
	ENVELOPE *e;
	extern ENVELOPE BlankEnvelope;

	CurHostName = mci->mci_host;		/* XXX UGLY XXX */
	if (CurHostName == NULL)
		CurHostName = MyHostName;

	e = &BlankEnvelope;
	SmtpPhase = "client probe";
	smtpmessage("RSET", m, mci);
	r = reply(m, mci, e, TimeOuts.to_miscshort, NULL, NULL);
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
reply(m, mci, e, timeout, pfunc, enhstat)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
	time_t timeout;
	void (*pfunc)();
	char **enhstat;
{
	register char *bufp;
	register int r;
	bool firstline = TRUE;
	char junkbuf[MAXLINE];
	static char enhstatcode[ENHSCLEN];
	int save_errno;

	if (mci->mci_out != NULL)
		(void) fflush(mci->mci_out);

	if (tTd(18, 1))
		dprintf("reply\n");

	/*
	**  Read the input line, being careful not to hang.
	*/

	bufp = SmtpReplyBuffer;
	for (;;)
	{
		register char *p;

		/* actually do the read */
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);	/* for debugging */

		/* if we are in the process of closing just give the code */
		if (mci->mci_state == MCIS_CLOSED)
			return SMTPCLOSING;

		if (mci->mci_out != NULL)
			(void) fflush(mci->mci_out);

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
			usrerr("451 4.4.1 reply: read error from %s",
			       CurHostName == NULL ? "NO_HOST" : CurHostName);

			/* errors on QUIT should not be persistent */
			if (strncmp(SmtpMsgBuffer, "QUIT", 4) != 0)
				mci_setstat(mci, EX_TEMPFAIL, "4.4.2", MsgBuf);

			/* if debugging, pause so we can see state */
			if (tTd(18, 100))
				(void) pause();
			mci->mci_state = MCIS_ERROR;
			save_errno = errno;
			smtpquit(m, mci, e);
# if XDEBUG
			{
				char wbuf[MAXLINE];
				int wbufleft = sizeof wbuf;

				p = wbuf;
				if (e->e_to != NULL)
				{
					int plen;

					snprintf(p, wbufleft, "%s... ",
						shortenstring(e->e_to, MAXSHORTSTR));
					plen = strlen(p);
					p += plen;
					wbufleft -= plen;
				}
				snprintf(p, wbufleft, "reply(%.100s) during %s",
					 CurHostName == NULL ? "NO_HOST" : CurHostName,
					 SmtpPhase);
				checkfd012(wbuf);
			}
# endif /* XDEBUG */
			errno = save_errno;
			HoldErrs = oldholderrs;
			return -1;
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
					CurHostName == NULL ? "NO_HOST" : CurHostName);
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

		/* ignore improperly formatted input */
		if (!ISSMTPREPLY(bufp))
			continue;

		if (bitset(MCIF_ENHSTAT, mci->mci_flags) &&
		    enhstat != NULL &&
		    extenhsc(bufp + 4, ' ', enhstatcode) > 0)
			*enhstat = enhstatcode;

		/* process the line */
		if (pfunc != NULL)
			(*pfunc)(bufp, firstline, m, mci, e);

		firstline = FALSE;

		/* decode the reply code */
		r = atoi(bufp);

		/* extra semantics: 0xx codes are "informational" */
		if (r < 100)
			continue;

		/* if no continuation lines, return this line */
		if (bufp[3] != '-')
			break;

		/* first line of real reply -- ignore rest */
		bufp = junkbuf;
	}

	/*
	**  Now look at SmtpReplyBuffer -- only care about the first
	**  line of the response from here on out.
	*/

	/* save temporary failure messages for posterity */
	if (SmtpReplyBuffer[0] == '4' &&
	    (bitnset(M_LMTP, m->m_flags) || SmtpError[0] == '\0'))
		snprintf(SmtpError, sizeof SmtpError, "%s", SmtpReplyBuffer);

	/* reply code 421 is "Service Shutting Down" */
	if (r == SMTPCLOSING && mci->mci_state != MCIS_SSD)
	{
		/* send the quit protocol */
		mci->mci_state = MCIS_SSD;
		smtpquit(m, mci, e);
	}

	return r;
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
# ifdef __STDC__
smtpmessage(char *f, MAILER *m, MCI *mci, ...)
# else /* __STDC__ */
smtpmessage(f, m, mci, va_alist)
	char *f;
	MAILER *m;
	MCI *mci;
	va_dcl
# endif /* __STDC__ */
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
		dprintf("smtpmessage: NULL mci_out\n");
	}
}

#endif /* SMTP */
