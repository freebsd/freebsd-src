/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: milter.c,v 8.50.4.30 2000/07/18 07:24:51 gshapiro Exp $";
#endif /* ! lint */

#if _FFR_MILTER

# include <sendmail.h>
# include <errno.h>
# include <sys/time.h>

# if NETINET || NETINET6
#  include <arpa/inet.h>
# endif /* NETINET || NETINET6 */


static void	milter_error __P((struct milter *));
static int	milter_open __P((struct milter *, bool, ENVELOPE *));
static void	milter_parse_timeouts __P((char *, struct milter *));

static char *MilterConnectMacros[MAXFILTERMACROS + 1];
static char *MilterHeloMacros[MAXFILTERMACROS + 1];
static char *MilterEnvFromMacros[MAXFILTERMACROS + 1];
static char *MilterEnvRcptMacros[MAXFILTERMACROS + 1];

# define MILTER_CHECK_DONE_MSG() \
	if (*state == SMFIR_REPLYCODE || \
	    *state == SMFIR_REJECT || \
	    *state == SMFIR_DISCARD || \
	    *state == SMFIR_TEMPFAIL) \
	{ \
		/* Abort the filters to let them know we are done with msg */ \
		milter_abort(e); \
	}

# define MILTER_CHECK_ERROR(action) \
	if (bitnset(SMF_TEMPFAIL, m->mf_flags)) \
		*state = SMFIR_TEMPFAIL; \
	else if (bitnset(SMF_REJECT, m->mf_flags)) \
		*state = SMFIR_REJECT; \
	else \
		action;

# define MILTER_CHECK_REPLYCODE(default) \
	if (response == NULL || \
	    strlen(response) + 1 != (size_t) rlen || \
	    rlen < 3 || \
	    (response[0] != '4' && response[0] != '5') || \
	    !isascii(response[1]) || !isdigit(response[1]) || \
	    !isascii(response[2]) || !isdigit(response[2])) \
	{ \
		if (response != NULL) \
			free(response); \
		response = newstr(default); \
	} \
	else \
	{ \
		char *ptr = response; \
 \
		/* Check for unprotected %'s in the string */ \
		while (*ptr != '\0') \
		{ \
			if (*ptr == '%' && *++ptr != '%') \
			{ \
				free(response); \
				response = newstr(default); \
				break; \
			} \
			ptr++; \
		} \
	}

# define MILTER_DF_ERROR(msg) \
{ \
	int save_errno = errno; \
 \
	if (tTd(64, 5)) \
	{ \
		dprintf(msg, dfname, errstring(save_errno)); \
		dprintf("\n"); \
	} \
	if (LogLevel > 0) \
		sm_syslog(LOG_ERR, e->e_id, msg, dfname, errstring(save_errno)); \
	if (SuperSafe) \
	{ \
		if (e->e_dfp != NULL) \
		{ \
			(void) fclose(e->e_dfp); \
			e->e_dfp = NULL; \
		} \
		e->e_flags &= ~EF_HAS_DF; \
	} \
	errno = save_errno; \
}

/*
**  MILTER_TIMEOUT -- make sure socket is ready in time
**
**	Parameters:
**		routine -- routine name for debug/logging
**		secs -- number of seconds in timeout
**		write -- waiting to read or write?
**
**	Assumes 'm' is a milter structure for the current socket.
*/


#  define MILTER_TIMEOUT(routine, secs, write) \
{ \
	int ret; \
	int save_errno; \
	fd_set fds; \
	struct timeval tv; \
 \
	if (m->mf_sock >= FD_SETSIZE) \
	{ \
		if (tTd(64, 5)) \
			dprintf("%s(%s): socket %d is larger than FD_SETSIZE %d\n", \
				routine, m->mf_name, m->mf_sock, FD_SETSIZE); \
		if (LogLevel > 0) \
			sm_syslog(LOG_ERR, e->e_id, \
				  "%s(%s): socket %d is larger than FD_SETSIZE %d\n", \
				  routine, m->mf_name, m->mf_sock, FD_SETSIZE); \
		milter_error(m); \
		return NULL; \
	} \
 \
	FD_ZERO(&fds); \
	FD_SET(m->mf_sock, &fds); \
	tv.tv_sec = secs; \
	tv.tv_usec = 0; \
	ret = select(m->mf_sock + 1, \
		     write ? NULL : &fds, \
		     write ? &fds : NULL, \
		     NULL, &tv); \
 \
	switch (ret) \
	{ \
	  case 0: \
		if (tTd(64, 5)) \
			dprintf("%s(%s): timeout\n", routine, m->mf_name); \
		if (LogLevel > 0) \
			sm_syslog(LOG_ERR, e->e_id, "%s(%s): timeout\n", \
				  routine, m->mf_name); \
		milter_error(m); \
		return NULL; \
 \
	  case -1: \
		save_errno = errno; \
		if (tTd(64, 5)) \
			dprintf("%s(%s): select: %s\n", \
				routine,  m->mf_name, errstring(save_errno)); \
		if (LogLevel > 0) \
			sm_syslog(LOG_ERR, e->e_id, \
				  "%s(%s): select: %s\n", \
				  routine, m->mf_name, errstring(save_errno)); \
		milter_error(m); \
		return NULL; \
 \
	  default: \
		if (FD_ISSET(m->mf_sock, &fds)) \
			break; \
		if (tTd(64, 5)) \
			dprintf("%s(%s): socket not ready\n", \
				routine, m->mf_name); \
		if (LogLevel > 0) \
			sm_syslog(LOG_ERR, e->e_id, \
				  "%s(%s): socket not ready\n", \
				  m->mf_name, routine); \
		milter_error(m); \
		return NULL; \
	} \
}


/*
**  Low level functions
*/

/*
**  MILTER_READ -- read from a remote milter filter
**
**	Parameters:
**		m -- milter to read from.
**		cmd -- return param for command read.
**		rlen -- return length of response string.
**		to -- timeout in seconds.
**		e -- current envelope.
**
**	Returns:
**		response string (may be NULL)
*/

static char *
milter_sysread(m, buf, sz, to, e)
	struct milter *m;
	char *buf;
	ssize_t sz;
	time_t to;
	ENVELOPE *e;
{
	time_t readstart;
	ssize_t len, curl;

	curl = 0;

	if (to > 0)
		readstart = curtime();

	for (;;)
	{
		if (to > 0)
		{
			time_t now;

			now = curtime();
			if (now - readstart >= to)
			{
				if (tTd(64, 5))
					dprintf("milter_read(%s): timeout before data read\n",
						m->mf_name);
				if (LogLevel > 0)
					sm_syslog(LOG_ERR, e->e_id,
						  "milter_read(%s): timeout before data read\n",
						  m->mf_name);
				milter_error(m);
				return NULL;
			}
			to -= now - readstart;
			readstart = now;
			MILTER_TIMEOUT("milter_read", to, FALSE);
		}

		len = read(m->mf_sock, buf + curl, sz - curl);

		if (len < 0)
		{
			int save_errno = errno;

			if (tTd(64, 5))
				dprintf("milter_read(%s): read returned %ld: %s\n",
					m->mf_name, (long) len,
					errstring(save_errno));
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, e->e_id,
					  "milter_read(%s): read returned %ld: %s",
					  m->mf_name, (long) len,
					  errstring(save_errno));
			milter_error(m);
			return NULL;
		}

		curl += len;
		if (len == 0 || len >= sz)
			break;

	}

	if (curl != sz)
	{
		if (tTd(64, 5))
			dprintf("milter_read(%s): read returned %ld, expecting %ld\n",
				m->mf_name, (long) curl, (long) sz);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_read(%s): read returned %ld, expecting %ld",
				  m->mf_name, (long) curl, (long) sz);
		milter_error(m);
		return NULL;
	}
	return buf;
}

static char *
milter_read(m, cmd, rlen, to, e)
	struct milter *m;
	char *cmd;
	ssize_t *rlen;
	time_t to;
	ENVELOPE *e;
{
	time_t readstart;
	ssize_t expl;
	mi_int32 i;
	char *buf;
	char data[MILTER_LEN_BYTES + 1];

	*rlen = 0;
	*cmd = '\0';

	if (to > 0)
		readstart = curtime();

	if (milter_sysread(m, data, sizeof data, to, e) == NULL)
		return NULL;

	/* reset timeout */
	if (to > 0)
	{
		time_t now;

		now = curtime();
		if (now - readstart >= to)
		{
			if (tTd(64, 5))
				dprintf("milter_read(%s): timeout before data read\n",
					m->mf_name);
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, e->e_id,
					  "milter_read(%s): timeout before data read\n",
					  m->mf_name);
			milter_error(m);
			return NULL;
		}
		to -= now - readstart;
	}

	*cmd = data[MILTER_LEN_BYTES];
	data[MILTER_LEN_BYTES] = '\0';
	(void) memcpy(&i, data, MILTER_LEN_BYTES);
	expl = ntohl(i) - 1;

	if (tTd(64, 25))
		dprintf("milter_read(%s): expecting %ld bytes\n",
			m->mf_name, (long) expl);

	if (expl < 0)
	{
		if (tTd(64, 5))
			dprintf("milter_read(%s): read size %ld out of range\n",
				m->mf_name, (long) expl);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_read(%s): read size %ld out of range",
				  m->mf_name, (long) expl);
		milter_error(m);
		return NULL;
	}

	if (expl == 0)
		return NULL;

	buf = (char *)xalloc(expl);

	if (milter_sysread(m, buf, expl, to, e) == NULL)
	{
		free(buf);
		return NULL;
	}

	if (tTd(64, 50))
		dprintf("milter_read(%s): Returning %*s\n",
			m->mf_name, (int) expl, buf);
	*rlen = expl;
	return buf;
}
/*
**  MILTER_WRITE -- write to a remote milter filter
**
**	Parameters:
**		m -- milter to read from.
**		cmd -- command to send.
**		buf -- optional command data.
**		len -- length of buf.
**		to -- timeout in seconds.
**		e -- current envelope.
**
**	Returns:
**		buf if successful, NULL otherwise
**		Not actually used anywhere but function prototype
**			must match milter_read()
*/

static char *
milter_write(m, cmd, buf, len, to, e)
	struct milter *m;
	char cmd;
	char *buf;
	ssize_t len;
	time_t to;
	ENVELOPE *e;
{
	time_t writestart = (time_t) 0;
	ssize_t sl, i;
	mi_int32 nl;
	char data[MILTER_LEN_BYTES + 1];

	if (len < 0 || len > MILTER_CHUNK_SIZE)
	{
		if (tTd(64, 5))
			dprintf("milter_write(%s): length %ld out of range\n",
				m->mf_name, (long) len);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_write(%s): length %ld out of range",
				  m->mf_name, (long) len);
		milter_error(m);
		return NULL;
	}

	if (tTd(64, 20))
		dprintf("milter_write(%s): cmd %c, len %ld\n",
			m->mf_name, cmd, (long) len);

	nl = htonl(len + 1);	/* add 1 for the cmd char */
	(void) memcpy(data, (char *) &nl, MILTER_LEN_BYTES);
	data[MILTER_LEN_BYTES] = cmd;
	sl = MILTER_LEN_BYTES + 1;

	if (to > 0)
	{
		writestart = curtime();
		MILTER_TIMEOUT("milter_write", to, TRUE);
	}

	/* use writev() instead to send the whole stuff at once? */
	i = write(m->mf_sock, (void *) data, sl);
	if (i != sl)
	{
		int save_errno = errno;

		if (tTd(64, 5))
			dprintf("milter_write(%s): write(%c) returned %ld, expected %ld: %s\n",
				m->mf_name, cmd, (long) i, (long) sl,
				errstring(save_errno));
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_write(%s): write(%c) returned %ld, expected %ld: %s",
				  m->mf_name, cmd, (long) i, (long) sl,
				  errstring(save_errno));
		milter_error(m);
		return buf;
	}

	if (len <= 0 || buf == NULL)
		return buf;

	if (tTd(64, 50))
		dprintf("milter_write(%s): Sending %*s\n",
			m->mf_name, (int) len, buf);

	if (to > 0)
	{
		time_t now;

		now = curtime();
		if (now - writestart >= to)
		{
			if (tTd(64, 5))
				dprintf("milter_write(%s): timeout before data send\n",
					m->mf_name);
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, e->e_id,
					  "milter_write(%s): timeout before data send\n",
					  m->mf_name);
			milter_error(m);
			return NULL;
		}
		else
		{
			to -= now - writestart;
			MILTER_TIMEOUT("milter_write", to, TRUE);
		}
	}

	i = write(m->mf_sock, (void *) buf, len);
	if (i != len)
	{
		int save_errno = errno;

		if (tTd(64, 5))
			dprintf("milter_write(%s): write(%c) returned %ld, expected %ld: %s\n",
				m->mf_name, cmd, (long) i, (long) sl,
				errstring(save_errno));
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_write(%s): write(%c) returned %ld, expected %ld: %s",
				  m->mf_name, cmd, (long) i, (long) len,
				  errstring(save_errno));
		milter_error(m);
		return NULL;
	}
	return buf;
}

/*
**  Utility functions
*/

/*
**  MILTER_OPEN -- connect to remote milter filter
**
**	Parameters:
**		m -- milter to connect to.
**		parseonly -- parse but don't connect.
**		e -- current envelope.
**
**	Returns:
**		connected socket if sucessful && !parseonly,
**		0 upon parse success if parseonly,
**		-1 otherwise.
*/

static int
milter_open(m, parseonly, e)
	struct milter *m;
	bool parseonly;
	ENVELOPE *e;
{
	int sock = 0;
	SOCKADDR_LEN_T addrlen = 0;
	int addrno = 0;
	int save_errno;
	char *p;
	char *colon;
	char *at;
	struct hostent *hp = NULL;
	SOCKADDR addr;

	if (m->mf_conn == NULL || m->mf_conn[0] == '\0')
	{
		if (tTd(64, 5))
			dprintf("X%s: empty or missing socket information\n",
				m->mf_name);
		if (parseonly)
			syserr("X%s: empty or missing socket information",
			       m->mf_name);
		else if (LogLevel > 10)
			sm_syslog(LOG_ERR, e->e_id,
				  "X%s: empty or missing socket information",
				  m->mf_name);
		milter_error(m);
		return -1;
	}

	/* protocol:filename or protocol:port@host */
	p = m->mf_conn;
	colon = strchr(p, ':');
	if (colon != NULL)
	{
		*colon = '\0';

		if (*p == '\0')
		{
# if NETUNIX
			/* default to AF_UNIX */
			addr.sa.sa_family = AF_UNIX;
# else /* NETUNIX */
#  if NETINET
			/* default to AF_INET */
			addr.sa.sa_family = AF_INET;
#  else /* NETINET */
#   if NETINET6
			/* default to AF_INET6 */
			addr.sa.sa_family = AF_INET6;
#   else /* NETINET6 */
			/* no protocols available */
			sm_syslog(LOG_ERR, e->e_id,
				  "X%s: no valid socket protocols available",
				  m->mf_name);
			milter_error(m);
			return -1;
#   endif /* NETINET6 */
#  endif /* NETINET */
# endif /* NETUNIX */
		}
# if NETUNIX
		else if (strcasecmp(p, "unix") == 0 ||
			 strcasecmp(p, "local") == 0)
			addr.sa.sa_family = AF_UNIX;
# endif /* NETUNIX */
# if NETINET
		else if (strcasecmp(p, "inet") == 0)
			addr.sa.sa_family = AF_INET;
# endif /* NETINET */
# if NETINET6
		else if (strcasecmp(p, "inet6") == 0)
			addr.sa.sa_family = AF_INET6;
# endif /* NETINET6 */
		else
		{
# ifdef EPROTONOSUPPORT
			errno = EPROTONOSUPPORT;
# else /* EPROTONOSUPPORT */
			errno = EINVAL;
# endif /* EPROTONOSUPPORT */
			if (tTd(64, 5))
				dprintf("X%s: unknown socket type %s\n",
					m->mf_name, p);
			if (parseonly)
				syserr("X%s: unknown socket type %s",
				       m->mf_name, p);
			else if (LogLevel > 10)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: unknown socket type %s",
					  m->mf_name, p);
			milter_error(m);
			return -1;
		}
		*colon++ = ':';
	}
	else
	{
		/* default to AF_UNIX */
		addr.sa.sa_family = AF_UNIX;
		colon = p;
	}

# if NETUNIX
	if (addr.sa.sa_family == AF_UNIX)
	{
		long sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_EXECOK;

		at = colon;
		if (strlen(colon) >= sizeof addr.sunix.sun_path)
		{
			if (tTd(64, 5))
				dprintf("X%s: local socket name %s too long\n",
					m->mf_name, colon);
			errno = EINVAL;
			if (parseonly)
				syserr("X%s: local socket name %s too long",
				       m->mf_name, colon);
			else if (LogLevel > 10)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: local socket name %s too long",
					  m->mf_name, colon);
			milter_error(m);
			return -1;
		}
		errno = safefile(colon, RunAsUid, RunAsGid, RunAsUserName, sff,
				 S_IRUSR|S_IWUSR, NULL);

		/* if just parsing .cf file, socket doesn't need to exist */
		if (parseonly && errno == ENOENT)
		{
			if (OpMode == MD_DAEMON ||
			    OpMode == MD_FGDAEMON)
				fprintf(stderr,
					"WARNING: X%s: local socket name %s missing\n",
					m->mf_name, colon);
		}
		else if (errno != 0)
		{
			/* if not safe, don't create */
			save_errno = errno;
			if (tTd(64, 5))
				dprintf("X%s: local socket name %s unsafe\n",
					m->mf_name, colon);
			errno = save_errno;
			if (parseonly)
			{
				if (OpMode == MD_DAEMON ||
				    OpMode == MD_FGDAEMON ||
				    OpMode == MD_SMTP)
					syserr("X%s: local socket name %s unsafe",
					       m->mf_name, colon);
			}
			else if (LogLevel > 10)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: local socket name %s unsafe",
					  m->mf_name, colon);
			milter_error(m);
			return -1;
		}

		(void) strlcpy(addr.sunix.sun_path, colon,
			       sizeof addr.sunix.sun_path);
		addrlen = sizeof (struct sockaddr_un);
	}
	else
# endif /* NETUNIX */
# if NETINET || NETINET6
	if (FALSE
#  if NETINET
		 || addr.sa.sa_family == AF_INET
#  endif /* NETINET */
#  if NETINET6
		 || addr.sa.sa_family == AF_INET6
#  endif /* NETINET6 */
		 )
	{
		u_short port;

		/* Parse port@host */
		at = strchr(colon, '@');
		if (at == NULL)
		{
			if (tTd(64, 5))
				dprintf("X%s: bad address %s (expected port@host)\n",
					m->mf_name, colon);
			if (parseonly)
				syserr("X%s: bad address %s (expected port@host)",
				       m->mf_name, colon);
			else if (LogLevel > 10)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: bad address %s (expected port@host)",
					  m->mf_name, colon);
			milter_error(m);
			return -1;
		}
		*at = '\0';
		if (isascii(*colon) && isdigit(*colon))
			port = htons((u_short) atoi(colon));
		else
		{
#  ifdef NO_GETSERVBYNAME
			if (tTd(64, 5))
				dprintf("X%s: invalid port number %s\n",
					m->mf_name, colon);
			if (parseonly)
				syserr("X%s: invalid port number %s",
				       m->mf_name, colon);
			else if (LogLevel > 10)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: invalid port number %s",
					  m->mf_name, colon);
			milter_error(m);
			return -1;
#  else /* NO_GETSERVBYNAME */
			register struct servent *sp;

			sp = getservbyname(colon, "tcp");
			if (sp == NULL)
			{
				save_errno = errno;
				if (tTd(64, 5))
					dprintf("X%s: unknown port name %s\n",
						m->mf_name, colon);
				errno = save_errno;
				if (parseonly)
					syserr("X%s: unknown port name %s",
					       m->mf_name, colon);
				else if (LogLevel > 10)
					sm_syslog(LOG_ERR, e->e_id,
						  "X%s: unknown port name %s",
						  m->mf_name, colon);
				milter_error(m);
				return -1;
			}
			port = sp->s_port;
#  endif /* NO_GETSERVBYNAME */
		}
		*at++ = '@';
		if (*at == '[')
		{
			char *end;

			end = strchr(at, ']');
			if (end != NULL)
			{
				bool found = FALSE;
#  if NETINET
				unsigned long hid = INADDR_NONE;
#  endif /* NETINET */
#  if NETINET6
				struct sockaddr_in6 hid6;
#  endif /* NETINET6 */

				*end = '\0';
#  if NETINET
				if (addr.sa.sa_family == AF_INET &&
				    (hid = inet_addr(&at[1])) != INADDR_NONE)
				{
					addr.sin.sin_addr.s_addr = hid;
					addr.sin.sin_port = port;
					found = TRUE;
				}
#  endif /* NETINET */
#  if NETINET6
				(void) memset(&hid6, '\0', sizeof hid6);
				if (addr.sa.sa_family == AF_INET6 &&
				    inet_pton(AF_INET6, &at[1],
					      &hid6.sin6_addr) == 1)
				{
					addr.sin6.sin6_addr = hid6.sin6_addr;
					addr.sin6.sin6_port = port;
					found = TRUE;
				}
#  endif /* NETINET6 */
				*end = ']';
				if (!found)
				{
					if (tTd(64, 5))
						dprintf("X%s: Invalid numeric domain spec \"%s\"\n",
							m->mf_name, at);
					if (parseonly)
						syserr("X%s: Invalid numeric domain spec \"%s\"",
						       m->mf_name, at);
					else if (LogLevel > 10)
						sm_syslog(LOG_ERR, e->e_id,
							  "X%s: Invalid numeric domain spec \"%s\"",
							  m->mf_name, at);
					milter_error(m);
					return -1;
				}
			}
			else
			{
				if (tTd(64, 5))
					dprintf("X%s: Invalid numeric domain spec \"%s\"\n",
						m->mf_name, at);
				if (parseonly)
					syserr("X%s: Invalid numeric domain spec \"%s\"",
					       m->mf_name, at);
				else if (LogLevel > 10)
					sm_syslog(LOG_ERR, e->e_id,
						  "X%s: Invalid numeric domain spec \"%s\"",
						  m->mf_name, at);
				milter_error(m);
				return -1;
			}
		}
		else
		{
			hp = sm_gethostbyname(at, addr.sa.sa_family);
			if (hp == NULL)
			{
				save_errno = errno;
				if (tTd(64, 5))
					dprintf("X%s: Unknown host name %s\n",
						m->mf_name, at);
				errno = save_errno;
				if (parseonly)
					syserr("X%s: Unknown host name %s",
					       m->mf_name, at);
				else if (LogLevel > 10)
					sm_syslog(LOG_ERR, e->e_id,
						  "X%s: Unknown host name %s",
						  m->mf_name, at);
				milter_error(m);
				return -1;
			}
			addr.sa.sa_family = hp->h_addrtype;
			switch (hp->h_addrtype)
			{
#  if NETINET
			  case AF_INET:
				memmove(&addr.sin.sin_addr,
					hp->h_addr,
					INADDRSZ);
				addr.sin.sin_port = port;
				addrlen = sizeof (struct sockaddr_in);
				addrno = 1;
				break;
#  endif /* NETINET */

#  if NETINET6
			  case AF_INET6:
				memmove(&addr.sin6.sin6_addr,
					hp->h_addr,
					IN6ADDRSZ);
				addr.sin6.sin6_port = port;
				addrlen = sizeof (struct sockaddr_in6);
				addrno = 1;
				break;
#  endif /* NETINET6 */

			  default:
				if (tTd(64, 5))
					dprintf("X%s: Unknown protocol for %s (%d)\n",
						m->mf_name, at,
						hp->h_addrtype);
				if (parseonly)
					syserr("X%s: Unknown protocol for %s (%d)",
					       m->mf_name, at, hp->h_addrtype);
				else if (LogLevel > 10)
					sm_syslog(LOG_ERR, e->e_id,
						  "X%s: Unknown protocol for %s (%d)",
						  m->mf_name, at,
						  hp->h_addrtype);
				milter_error(m);
				return -1;
			}
		}
	}
	else
# endif /* NETINET || NETINET6 */
	{
		if (tTd(64, 5))
			dprintf("X%s: unknown socket protocol\n", m->mf_name);
		if (parseonly)
			syserr("X%s: unknown socket protocol", m->mf_name);
		else if (LogLevel > 10)
			sm_syslog(LOG_ERR, e->e_id,
				  "X%s: unknown socket protocol", m->mf_name);
		milter_error(m);
		return -1;
	}

	/* just parsing through? */
	if (parseonly)
	{
		m->mf_state = SMFS_READY;
		return 0;
	}

	/* sanity check */
	if (m->mf_state != SMFS_READY &&
	    m->mf_state != SMFS_CLOSED)
	{
		/* shouldn't happen */
		if (tTd(64, 1))
			dprintf("milter_open(%s): Trying to open filter in state %c\n",
				m->mf_name, (char) m->mf_state);
		milter_error(m);
		return -1;
	}

	/* nope, actually connecting */
	for (;;)
	{
		sock = socket(addr.sa.sa_family, SOCK_STREAM, 0);
		if (sock < 0)
		{
			save_errno = errno;
			if (tTd(64, 5))
				dprintf("X%s: error creating socket: %s\n",
					m->mf_name, errstring(save_errno));
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, e->e_id,
					  "X%s: error creating socket: %s",
					  m->mf_name, errstring(save_errno));
			milter_error(m);
			return -1;
		}

		if (connect(sock, (struct sockaddr *) &addr, addrlen) >= 0)
			break;

		/* couldn't connect.... try next address */
		save_errno = errno;
		if (tTd(64, 5))
			dprintf("milter_open(%s): %s failed: %s\n",
				m->mf_name, at, errstring(save_errno));
		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, e->e_id,
				  "milter_open(%s): %s failed: %s",
				  m->mf_name, at, errstring(save_errno));
		(void) close(sock);

		/* try next address */
		if (hp != NULL && hp->h_addr_list[addrno] != NULL)
		{
			switch (addr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				memmove(&addr.sin.sin_addr,
					hp->h_addr_list[addrno++],
					INADDRSZ);
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				memmove(&addr.sin6.sin6_addr,
					hp->h_addr_list[addrno++],
					IN6ADDRSZ);
				break;
# endif /* NETINET6 */

			  default:
				if (tTd(64, 5))
					dprintf("X%s: Unknown protocol for %s (%d)\n",
						m->mf_name, at,
						hp->h_addrtype);
				if (LogLevel > 0)
					sm_syslog(LOG_ERR, e->e_id,
						  "X%s: Unknown protocol for %s (%d)",
						  m->mf_name, at,
						  hp->h_addrtype);
				milter_error(m);
				return -1;
			}
			continue;
		}
		if (tTd(64, 5))
			dprintf("X%s: error connecting to filter\n",
				m->mf_name);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "X%s: error connecting to filter",
				  m->mf_name);
		milter_error(m);
		return -1;
	}
	m->mf_state = SMFS_OPEN;
	return sock;
}
/*
**  MILTER_SETUP -- setup structure for a mail filter
**
**	Parameters:
**		line -- the options line.
**
**	Returns:
**		none
*/

void
milter_setup(line)
	char *line;
{
	char fcode;
	register char *p;
	register struct milter *m;
	STAB *s;

	/* collect the mailer name */
	for (p = line;
	     *p != '\0' && *p != ',' && !(isascii(*p) && isspace(*p));
	     p++)
		continue;
	if (*p != '\0')
		*p++ = '\0';
	if (line[0] == '\0')
	{
		syserr("name required for mail filter");
		return;
	}
	m = (struct milter *)xalloc(sizeof *m);
	memset((char *) m, '\0', sizeof *m);
	m->mf_name = newstr(line);
	m->mf_state = SMFS_READY;
	m->mf_sock = -1;
	m->mf_timeout[SMFTO_WRITE] = (time_t) 10;
	m->mf_timeout[SMFTO_READ] = (time_t) 10;
	m->mf_timeout[SMFTO_EOM] = (time_t) 300;

	/* now scan through and assign info from the fields */
	while (*p != '\0')
	{
		char *delimptr;

		while (*p != '\0' &&
		       (*p == ',' || (isascii(*p) && isspace(*p))))
			p++;

		/* p now points to field code */
		fcode = *p;
		while (*p != '\0' && *p != '=' && *p != ',')
			p++;
		if (*p++ != '=')
		{
			syserr("X%s: `=' expected", m->mf_name);
			return;
		}
		while (isascii(*p) && isspace(*p))
			p++;

		/* p now points to the field body */
		p = munchstring(p, &delimptr, ',');

		/* install the field into the mailer struct */
		switch (fcode)
		{
		  case 'S':		/* socket */
			if (p == NULL)
				m->mf_conn = NULL;
			else
				m->mf_conn = newstr(p);
			break;

		  case 'F':		/* Milter flags configured on MTA */
			for (; *p != '\0'; p++)
			{
				if (!(isascii(*p) && isspace(*p)))
					setbitn(*p, m->mf_flags);
			}
			break;

		  case 'T':		/* timeouts */
			milter_parse_timeouts(p, m);
			break;

		  default:
			syserr("X%s: unknown filter equate %c=",
			       m->mf_name, fcode);
			break;
		}
		p = delimptr;
	}

	/* early check for errors */
	(void) milter_open(m, TRUE, CurEnv);

	/* enter the mailer into the symbol table */
	s = stab(m->mf_name, ST_MILTER, ST_ENTER);
	if (s->s_milter != NULL)
		syserr("X%s: duplicate filter definition", m->mf_name);
	else
		s->s_milter = m;
}
/*
**  MILTER_PARSE_LIST -- parse option list into an array
**
**	Called when reading configuration file.
**
**	Parameters:
**		spec -- the filter list.
**		list -- the array to fill in.
**		max -- the maximum number of entries in list.
**
**	Returns:
**		none
*/

void
milter_parse_list(spec, list, max)
	char *spec;
	struct milter **list;
	int max;
{
	int numitems = 0;
	register char *p;

	/* leave one for the NULL signifying the end of the list */
	max--;

	for (p = spec; p != NULL; )
	{
		STAB *s;

		while (isascii(*p) && isspace(*p))
			p++;
		if (*p == '\0')
			break;
		spec = p;

		if (numitems >= max)
		{
			syserr("Too many filters defined, %d max", max);
			if (max > 0)
				list[0] = NULL;
			return;
		}
		p = strpbrk(p, ",");
		if (p != NULL)
			*p++ = '\0';

		s = stab(spec, ST_MILTER, ST_FIND);
		if (s == NULL)
		{
			syserr("InputFilter %s not defined", spec);
			ExitStat = EX_CONFIG;
			return;
		}
		list[numitems++] = s->s_milter;
	}
	list[numitems] = NULL;
}
/*
**  MILTER_PARSE_TIMEOUTS -- parse timeout list
**
**	Called when reading configuration file.
**
**	Parameters:
**		spec -- the timeout list.
**		m -- milter to set.
**
**	Returns:
**		none
*/

static void
milter_parse_timeouts(spec, m)
	char *spec;
	struct milter *m;
{
	char fcode;
	register char *p;

	p = spec;

	/* now scan through and assign info from the fields */
	while (*p != '\0')
	{
		char *delimptr;

		while (*p != '\0' &&
		       (*p == ';' || (isascii(*p) && isspace(*p))))
			p++;

		/* p now points to field code */
		fcode = *p;
		while (*p != '\0' && *p != ':')
			p++;
		if (*p++ != ':')
		{
			syserr("X%s, T=: `:' expected", m->mf_name);
			return;
		}
		while (isascii(*p) && isspace(*p))
			p++;

		/* p now points to the field body */
		p = munchstring(p, &delimptr, ';');

		/* install the field into the mailer struct */
		switch (fcode)
		{
		  case 'S':
			m->mf_timeout[SMFTO_WRITE] = convtime(p, 's');
			if (tTd(64, 5))
				printf("X%s: %c=%ld\n",
				       m->mf_name, fcode,
				       (u_long) m->mf_timeout[SMFTO_WRITE]);
			break;

		  case 'R':
			m->mf_timeout[SMFTO_READ] = convtime(p, 's');
			if (tTd(64, 5))
				printf("X%s: %c=%ld\n",
				       m->mf_name, fcode,
				       (u_long) m->mf_timeout[SMFTO_READ]);
			break;

		  case 'E':
			m->mf_timeout[SMFTO_EOM] = convtime(p, 's');
			if (tTd(64, 5))
				printf("X%s: %c=%ld\n",
				       m->mf_name, fcode,
				       (u_long) m->mf_timeout[SMFTO_EOM]);
			break;

		  default:
			if (tTd(64, 5))
				printf("X%s: %c unknown\n",
				       m->mf_name, fcode);
			syserr("X%s: unknown filter timeout %c",
			       m->mf_name, fcode);
			break;
		}
		p = delimptr;
	}
}
/*
**  MILTER_SET_OPTION -- set an individual milter option
**
**	Parameters:
**		name -- the name of the option.
**		val -- the value of the option.
**		sticky -- if set, don't let other setoptions override
**			this value.
**
**	Returns:
**		none.
*/

/* set if Milter sub-option is stuck */
static BITMAP256	StickyMilterOpt;

static struct milteropt
{
	char	*mo_name;	/* long name of milter option */
	u_char	mo_code;	/* code for option */
} MilterOptTab[] =
{
# define MO_MACROS_CONNECT		0x01
	{ "macros.connect",		MO_MACROS_CONNECT		},
# define MO_MACROS_HELO			0x02
	{ "macros.helo",		MO_MACROS_HELO			},
# define MO_MACROS_ENVFROM		0x03
	{ "macros.envfrom",		MO_MACROS_ENVFROM		},
# define MO_MACROS_ENVRCPT		0x04
	{ "macros.envrcpt",		MO_MACROS_ENVRCPT		},
	{ NULL,				0				},
};

void
milter_set_option(name, val, sticky)
	char *name;
	char *val;
	bool sticky;
{
	int nummac = 0;
	register struct milteropt *mo;
	char *p;
	char **macros = NULL;

	if (tTd(37, 2) || tTd(64, 5))
		dprintf("milter_set_option(%s = %s)", name, val);

	for (mo = MilterOptTab; mo->mo_name != NULL; mo++)
	{
		if (strcasecmp(mo->mo_name, name) == 0)
			break;
	}

	if (mo->mo_name == NULL)
		syserr("milter_set_option: invalid Milter option %s", name);

	/*
	**  See if this option is preset for us.
	*/

	if (!sticky && bitnset(mo->mo_code, StickyMilterOpt))
	{
		if (tTd(37, 2) || tTd(64,5))
			dprintf(" (ignored)\n");
		return;
	}

	if (tTd(37, 2) || tTd(64,5))
		dprintf("\n");

	switch (mo->mo_code)
	{
	  case MO_MACROS_CONNECT:
		if (macros == NULL)
			macros = MilterConnectMacros;
		/* FALLTHROUGH */

	  case MO_MACROS_HELO:
		if (macros == NULL)
			macros = MilterHeloMacros;
		/* FALLTHROUGH */

	  case MO_MACROS_ENVFROM:
		if (macros == NULL)
			macros = MilterEnvFromMacros;
		/* FALLTHROUGH */

	  case MO_MACROS_ENVRCPT:
		if (macros == NULL)
			macros = MilterEnvRcptMacros;

		p = newstr(val);
		while (*p != '\0')
		{
			char *macro;

			/* Skip leading commas, spaces */
			while (*p != '\0' &&
			       (*p == ',' || (isascii(*p) && isspace(*p))))
				p++;

			if (*p == '\0')
				break;

			/* Find end of macro */
			macro = p;
			while (*p != '\0' && *p != ',' &&
			       isascii(*p) && !isspace(*p))
				p++;
			if (*p != '\0')
				*p++ = '\0';

			if (nummac >= MAXFILTERMACROS)
			{
				syserr("milter_set_option: too many macros in Milter.%s (max %d)",
				       name, MAXFILTERMACROS);
				macros[nummac] = NULL;
				break;
			}
			macros[nummac++] = macro;
		}
		macros[nummac] = NULL;
		break;

	  default:
		syserr("milter_set_option: invalid Milter option %s", name);
		break;
	}

	if (sticky)
		setbitn(mo->mo_code, StickyMilterOpt);
}
/*
**  MILTER_REOPEN_DF -- open & truncate the df file (for replbody)
**
**	Parameters:
**		e -- current envelope.
**
**	Returns:
**		0 if succesful, -1 otherwise
*/

static int
milter_reopen_df(e)
	ENVELOPE *e;
{
	char dfname[MAXPATHLEN];

	(void) strlcpy(dfname, queuename(e, 'd'), sizeof dfname);

	/*
	**  In SuperSafe mode, e->e_dfp is a read-only FP so
	**  close and reopen writable (later close and reopen
	**  read only again).
	**
	**  In !SuperSafe mode, e->e_dfp still points at the
	**  buffered file I/O descriptor, still open for writing
	**  so there isn't as much work to do, just truncate it
	**  and go.
	*/

	if (SuperSafe)
	{
		/* close read-only df */
		if (bitset(EF_HAS_DF, e->e_flags) && e->e_dfp != NULL)
		{
			(void) fclose(e->e_dfp);
			e->e_flags &= ~EF_HAS_DF;
		}

		/* open writable */
		if ((e->e_dfp = fopen(dfname, "w+")) == NULL)
		{
			MILTER_DF_ERROR("milter_reopen_df: fopen %s: %s");
			return -1;
		}
	}
	else if (e->e_dfp == NULL)
	{
		/* shouldn't happen */
		errno = ENOENT;
		MILTER_DF_ERROR("milter_reopen_df: NULL e_dfp (%s: %s)");
		return -1;
	}
	return 0;
}
/*
**  MILTER_RESET_DF -- re-open read-only the df file (for replbody)
**
**	Parameters:
**		e -- current envelope.
**
**	Returns:
**		0 if succesful, -1 otherwise
*/

static int
milter_reset_df(e)
	ENVELOPE *e;
{
	int afd;
	char dfname[MAXPATHLEN];

	(void) strlcpy(dfname, queuename(e, 'd'), sizeof dfname);

	if (fflush(e->e_dfp) != 0 || ferror(e->e_dfp))
	{
		MILTER_DF_ERROR("milter_reset_df: error writing/flushing %s: %s");
		return -1;
	}
	else if (!SuperSafe)
	{
		/* skip next few clauses */
		/* EMPTY */
	}
	else if ((afd = fileno(e->e_dfp)) >= 0 && fsync(afd) < 0)
	{
		MILTER_DF_ERROR("milter_reset_df: error sync'ing %s: %s");
		return -1;
	}
	else if (fclose(e->e_dfp) < 0)
	{
		MILTER_DF_ERROR("milter_reset_df: error closing %s: %s");
		return -1;
	}
	else if ((e->e_dfp = fopen(dfname, "r")) == NULL)
	{
		MILTER_DF_ERROR("milter_reset_df: error reopening %s: %s");
		return -1;
	}
	else
		e->e_flags |= EF_HAS_DF;
	return 0;
}
/*
**  MILTER_CAN_DELRCPTS -- can any milter filters delete recipients?
**
**	Parameters:
**		none
**
**	Returns:
**		TRUE if any filter deletes recipients, FALSE otherwise
*/

bool
milter_can_delrcpts()
{
	bool can = FALSE;
	int i;

	if (tTd(64, 10))
		dprintf("milter_can_delrcpts:");

	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		if (bitset(SMFIF_DELRCPT, m->mf_fflags))
		{
			can = TRUE;
			break;
		}
	}
	if (tTd(64, 10))
		dprintf("%s\n", can ? "TRUE" : "FALSE");

	return can;
}
/*
**  MILTER_QUIT_FILTER -- close down a single filter
**
**	Parameters:
**		m -- milter structure of filter to close down.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_quit_filter(m, e)
	struct milter *m;
	ENVELOPE *e;
{
	if (tTd(64, 10))
		dprintf("milter_quit_filter(%s)\n", m->mf_name);

	/* Never replace error state */
	if (m->mf_state == SMFS_ERROR)
		return;

	if (m->mf_sock < 0 ||
	    m->mf_state == SMFS_CLOSED ||
	    m->mf_state == SMFS_READY)
	{
		m->mf_sock = -1;
		m->mf_state = SMFS_CLOSED;
		return;
	}

	(void) milter_write(m, SMFIC_QUIT, (char *) NULL, 0,
			    m->mf_timeout[SMFTO_WRITE], e);
	(void) close(m->mf_sock);
	m->mf_sock = -1;
	if (m->mf_state != SMFS_ERROR)
		m->mf_state = SMFS_CLOSED;
}
/*
**  MILTER_ABORT_FILTER -- tell filter to abort current message
**
**	Parameters:
**		m -- milter structure of filter to abort.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_abort_filter(m, e)
	struct milter *m;
	ENVELOPE *e;
{
	if (tTd(64, 10))
		dprintf("milter_abort_filter(%s)\n", m->mf_name);

	if (m->mf_sock < 0 ||
	    m->mf_state != SMFS_INMSG)
		return;

	(void) milter_write(m, SMFIC_ABORT, (char *) NULL, 0,
			    m->mf_timeout[SMFTO_WRITE], e);
	if (m->mf_state != SMFS_ERROR)
		m->mf_state = SMFS_DONE;
}
/*
**  MILTER_SEND_MACROS -- provide macros to the filters
**
**	Parameters:
**		m -- milter to send macros to.
**		macros -- macros to send for filter smfi_getsymval().
**		cmd -- which command the macros are associated with.
**		e -- current envelope (for macro access).
**
**	Returns:
**		none
*/

static void
milter_send_macros(m, macros, cmd, e)
	struct milter *m;
	char **macros;
	char cmd;
	ENVELOPE *e;
{
	int i;
	int mid;
	char *v;
	char *buf, *bp;
	ssize_t s;

	/* sanity check */
	if (macros == NULL || macros[0] == NULL)
		return;

	/* put together data */
	s = 1;			/* for the command character */
	for (i = 0; macros[i] != NULL; i++)
	{
		mid = macid(macros[i], NULL);
		if (mid == '\0')
			continue;
		v = macvalue(mid, e);
		if (v == NULL)
			continue;
		s += strlen(macros[i]) + 1 + strlen(v) + 1;
	}

	buf = (char *)xalloc(s);
	bp = buf;
	*bp++ = cmd;
	for (i = 0; macros[i] != NULL; i++)
	{
		mid = macid(macros[i], NULL);
		if (mid == '\0')
			continue;
		v = macvalue(mid, e);
		if (v == NULL)
			continue;

		if (tTd(64, 10))
			dprintf("milter_send_macros(%s, %c): %s=%s\n",
				m->mf_name, cmd, macros[i], v);

		(void) strlcpy(bp, macros[i], s - (bp - buf));
		bp += strlen(bp) + 1;
		(void) strlcpy(bp, v, s - (bp - buf));
		bp += strlen(bp) + 1;
	}
	(void) milter_write(m, SMFIC_MACRO, buf, s,
			    m->mf_timeout[SMFTO_WRITE], e);
	free(buf);
}

/*
**  MILTER_SEND_COMMAND -- send a command and return the response for a filter
**
**	Parameters:
**		m -- current milter filter
**		command -- command to send.
**		data -- optional command data.
**		sz -- length of buf.
**		e -- current envelope (for e->e_id).
**		state -- return state word.
**
**	Returns:
**		response string (may be NULL)
*/

static char *
milter_send_command(m, command, data, sz, e, state)
	struct milter *m;
	char command;
	void *data;
	ssize_t sz;
	ENVELOPE *e;
	char *state;
{
	char rcmd;
	ssize_t rlen;
	u_long skipflag;
	char *defresponse;
	char *response;

	if (tTd(64, 10))
		dprintf("milter_send_command(%s): cmd %c len %ld\n",
			m->mf_name, (char) command, (long) sz);

	/* find skip flag and default failure */
	switch (command)
	{
	  case SMFIC_CONNECT:
		skipflag = SMFIP_NOCONNECT;
		defresponse = "554 Command rejected";
		break;

	  case SMFIC_HELO:
		skipflag = SMFIP_NOHELO;
		defresponse = "550 Command rejected";
		break;

	  case SMFIC_MAIL:
		skipflag = SMFIP_NOMAIL;
		defresponse = "550 5.7.1 Command rejected";
		break;

	  case SMFIC_RCPT:
		skipflag = SMFIP_NORCPT;
		defresponse = "550 5.7.1 Command rejected";
		break;

	  case SMFIC_HEADER:
		skipflag = SMFIP_NOHDRS;
		defresponse = "550 5.7.1 Command rejected";
		break;

	  case SMFIC_BODY:
		skipflag = SMFIP_NOBODY;
		defresponse = "554 5.7.1 Command rejected";
		break;

	  case SMFIC_EOH:
		skipflag = SMFIP_NOEOH;
		defresponse = "550 5.7.1 Command rejected";
		break;

	  case SMFIC_BODYEOB:
	  case SMFIC_OPTNEG:
	  case SMFIC_MACRO:
	  case SMFIC_ABORT:
	  case SMFIC_QUIT:
		/* NOTE: not handled by milter_send_command() */
		/* FALLTHROUGH */

	  default:
		skipflag = 0;
		defresponse = "550 5.7.1 Command rejected";
		break;
	}

	/* check if filter wants this command */
	if (skipflag != 0 &&
	    bitset(skipflag, m->mf_pflags))
		return NULL;


	(void) milter_write(m, command, data, sz,
			    m->mf_timeout[SMFTO_WRITE], e);
	if (m->mf_state == SMFS_ERROR)
	{
		MILTER_CHECK_ERROR(/* EMPTY */;);
		return NULL;
	}

	response = milter_read(m, &rcmd, &rlen,
			       m->mf_timeout[SMFTO_READ], e);
	if (m->mf_state == SMFS_ERROR)
	{
		MILTER_CHECK_ERROR(/* EMPTY */;);
		return NULL;
	}

	if (tTd(64, 10))
		dprintf("milter_send_command(%s): returned %c\n",
			m->mf_name, (char) rcmd);

	switch (rcmd)
	{
	  case SMFIR_REPLYCODE:
		MILTER_CHECK_REPLYCODE(defresponse);
		/* FALLTHROUGH */

	  case SMFIR_REJECT:
	  case SMFIR_DISCARD:
	  case SMFIR_TEMPFAIL:
		*state = rcmd;
		break;

	  case SMFIR_ACCEPT:
		/* this filter is done with message/connection */
		m->mf_state = SMFS_DONE;
		break;

	  case SMFIR_CONTINUE:
		/* if MAIL command is ok, filter is in message state */
		if (command == SMFIC_MAIL)
			m->mf_state = SMFS_INMSG;
		break;

	  default:
		/* Invalid response to command */
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_send_command(%s): returned bogus response %c",
				  m->mf_name, rcmd);
		milter_error(m);
		break;
	}

	if (*state != SMFIR_REPLYCODE &&
	    response != NULL)
	{
		free(response);
		response = NULL;
	}
	return response;
}

/*
**  MILTER_COMMAND -- send a command and return the response for each filter
**
**	Parameters:
**		command -- command to send.
**		data -- optional command data.
**		sz -- length of buf.
**		macros -- macros to send for filter smfi_getsymval().
**		e -- current envelope (for macro access).
**		state -- return state word.
**
**	Returns:
**		response string (may be NULL)
*/

static char *
milter_command(command, data, sz, macros, e, state)
	char command;
	void *data;
	ssize_t sz;
	char **macros;
	ENVELOPE *e;
	char *state;
{
	int i;
	char *response = NULL;

	if (tTd(64, 10))
		dprintf("milter_command: cmd %c len %ld\n",
			(char) command, (long) sz);

	*state = SMFIR_CONTINUE;
	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		/* sanity check */
		if (m->mf_sock < 0 ||
		    (m->mf_state != SMFS_OPEN && m->mf_state != SMFS_INMSG))
			continue;

		/* send macros (regardless of whether we send command) */
		if (macros != NULL && macros[0] != NULL)
		{
			milter_send_macros(m, macros, command, e);
			if (m->mf_state == SMFS_ERROR)
			{
				MILTER_CHECK_ERROR(continue);
				break;
			}
		}

		response = milter_send_command(m, command, data, sz, e, state);
		if (*state != SMFIR_CONTINUE)
			break;
	}
	return response;
}
/*
**  MILTER_NEGOTIATE -- get version and flags from filter
**
**	Parameters:
**		m -- milter filter structure.
**		e -- current envelope.
**
**	Returns:
**		0 on success, -1 otherwise
*/

static int
milter_negotiate(m, e)
	struct milter *m;
	ENVELOPE *e;
{
	char rcmd;
	mi_int32 fvers;
	mi_int32 fflags;
	mi_int32 pflags;
	char *response;
	ssize_t rlen;
	char data[MILTER_OPTLEN];

	/* sanity check */
	if (m->mf_sock < 0 || m->mf_state != SMFS_OPEN)
	{
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): impossible state",
				  m->mf_name);
		milter_error(m);
		return -1;
	}

	fvers = htonl(SMFI_VERSION);
	fflags = htonl(SMFI_CURR_ACTS);
	pflags = htonl(SMFI_CURR_PROT);
	(void) memcpy(data, (char *) &fvers, MILTER_LEN_BYTES);
	(void) memcpy(data + MILTER_LEN_BYTES,
		      (char *) &fflags, MILTER_LEN_BYTES);
	(void) memcpy(data + (MILTER_LEN_BYTES * 2),
		      (char *) &pflags, MILTER_LEN_BYTES);
	(void) milter_write(m, SMFIC_OPTNEG, data, sizeof data,
			    m->mf_timeout[SMFTO_WRITE], e);

	if (m->mf_state == SMFS_ERROR)
		return -1;

	response = milter_read(m, &rcmd, &rlen, m->mf_timeout[SMFTO_READ], e);
	if (m->mf_state == SMFS_ERROR)
		return -1;

	if (rcmd != SMFIC_OPTNEG)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): returned %c instead of %c\n",
				m->mf_name, rcmd, SMFIC_OPTNEG);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): returned %c instead of %c",
				  m->mf_name, rcmd, SMFIC_OPTNEG);
		if (response != NULL)
			free(response);
		milter_error(m);
		return -1;
	}

	/* Make sure we have enough bytes for the version */
	if (response == NULL || rlen < MILTER_LEN_BYTES)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): did not return valid info\n",
				m->mf_name);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): did not return valid info",
				  m->mf_name);
		if (response != NULL)
			free(response);
		milter_error(m);
		return -1;
	}

	/* extract information */
	(void) memcpy((char *) &fvers, response, MILTER_LEN_BYTES);

	/* Now make sure we have enough for the feature bitmap */
	if (rlen != MILTER_OPTLEN)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): did not return enough info\n",
				m->mf_name);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): did not return enough info",
				  m->mf_name);
		if (response != NULL)
			free(response);
		milter_error(m);
		return -1;
	}

	(void) memcpy((char *) &fflags, response + MILTER_LEN_BYTES,
		      MILTER_LEN_BYTES);
	(void) memcpy((char *) &pflags, response + (MILTER_LEN_BYTES * 2),
		      MILTER_LEN_BYTES);
	free(response);
	response = NULL;

	m->mf_fvers = ntohl(fvers);
	m->mf_fflags = ntohl(fflags);
	m->mf_pflags = ntohl(pflags);

	/* check for version compatibility */
	if (m->mf_fvers == 1 ||
	    m->mf_fvers > SMFI_VERSION)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): version %lu != MTA milter version %d\n",
				m->mf_name, m->mf_fvers, SMFI_VERSION);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): version %ld != MTA milter version %d",
				  m->mf_name, m->mf_fvers, SMFI_VERSION);
		milter_error(m);
		return -1;
	}

	/* check for filter feature mismatch */
	if ((m->mf_fflags & SMFI_CURR_ACTS) != m->mf_fflags)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): filter abilities 0x%lx != MTA milter abilities 0x%lx\n",
				m->mf_name, m->mf_fflags,
				(u_long) SMFI_CURR_ACTS);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): filter abilities 0x%lx != MTA milter abilities 0x%lx\n",
				  m->mf_name, m->mf_fflags,
				  (u_long) SMFI_CURR_ACTS);
		milter_error(m);
		return -1;
	}

	/* check for protocol feature mismatch */
	if ((m->mf_pflags & SMFI_CURR_PROT) != m->mf_pflags)
	{
		if (tTd(64, 5))
			dprintf("milter_negotiate(%s): protocol abilities 0x%lx != MTA milter abilities 0x%lx\n",
				m->mf_name, m->mf_pflags,
				(u_long) SMFI_CURR_PROT);
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, e->e_id,
				  "milter_negotiate(%s): protocol abilities 0x%lx != MTA milter abilities 0x%lx\n",
				  m->mf_name, m->mf_pflags,
				  (u_long) SMFI_CURR_PROT);
		milter_error(m);
		return -1;
	}

	if (tTd(64, 5))
		dprintf("milter_negotiate(%s): version %lu, fflags 0x%lx, pflags 0x%lx\n",
			m->mf_name, m->mf_fvers, m->mf_fflags, m->mf_pflags);
	return 0;
}
/*
**  MILTER_PER_CONNECTION_CHECK -- checks on per-connection commands
**
**	Reduce code duplication by putting these checks in one place
**
**	Parameters:
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_per_connection_check(e)
	ENVELOPE *e;
{
	int i;

	/* see if we are done with any of the filters */
	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		if (m->mf_state == SMFS_DONE)
			milter_quit_filter(m, e);
	}
}
/*
**  MILTER_ERROR -- Put a milter filter into error state
**
**	Parameters:
**		m -- the broken filter.
**
**	Returns:
**		none
*/

static void
milter_error(m)
	struct milter *m;
{
	/*
	**  We could send a quit here but
	**  we may have gotten here due to
	**  an I/O error so we don't want
	**  to try to make things worse.
	*/

	if (m->mf_sock >= 0)
	{
		(void) close(m->mf_sock);
		m->mf_sock = -1;
	}
	m->mf_state = SMFS_ERROR;
}
/*
**  MILTER_HEADERS -- send headers to a single milter filter
**
**	Parameters:
**		m -- current filter.
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

static char *
milter_headers(m, e, state)
	struct milter *m;
	ENVELOPE *e;
	char *state;
{
	char *response = NULL;
	HDR *h;

	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		char *buf;
		ssize_t s;

		/* don't send over deleted headers */
		if (h->h_value == NULL)
		{
			/* strip H_USER so not counted in milter_chgheader() */
			h->h_flags &= ~H_USER;
			continue;
		}

		/* skip auto-generated */
		if (!bitset(H_USER, h->h_flags))
			continue;

		if (tTd(64, 10))
			dprintf("milter_headers: %s: %s\n",
				h->h_field, h->h_value);

		s = strlen(h->h_field) + 1 +
			strlen(h->h_value) + 1;
		buf = (char *) xalloc(s);
		snprintf(buf, s, "%s%c%s", h->h_field, '\0', h->h_value);

		/* send it over */
		response = milter_send_command(m, SMFIC_HEADER, buf,
					       s, e, state);
		free(buf);
		if (m->mf_state == SMFS_ERROR ||
		    m->mf_state == SMFS_DONE ||
		    *state != SMFIR_CONTINUE)
			break;
	}
	return response;
}
/*
**  MILTER_BODY -- send the body to a filter
**
**	Parameters:
**		m -- current filter.
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

static char *
milter_body(m, e, state)
	struct milter *m;
	ENVELOPE *e;
	char *state;
{
	char bufchar = '\0';
	char prevchar = '\0';
	int c;
	char *response = NULL;
	char *bp;
	char buf[MILTER_CHUNK_SIZE];

	if (tTd(64, 10))
		dprintf("milter_body\n");

	if (bfrewind(e->e_dfp) < 0)
	{
		ExitStat = EX_IOERR;
		*state = SMFIR_TEMPFAIL;
		syserr("milter_body: %s/df%s: rewind error",
		       qid_printqueue(e->e_queuedir), e->e_id);
		return NULL;
	}

	bp = buf;
	while ((c = getc(e->e_dfp)) != EOF)
	{
		/*  Change LF to CRLF */
		if (c == '\n')
		{
			/* Not a CRLF already? */
			if (prevchar != '\r')
			{
				/* Room for CR now? */
				if (bp + 2 > &buf[sizeof buf])
				{
					/* No room, buffer LF */
					bufchar = c;

					/* and send CR now */
					c = '\r';
				}
				else
				{
					/* Room to do it now */
					*bp++ = '\r';
					prevchar = '\r';
				}
			}
		}
		*bp++ = (char) c;
		prevchar = c;
		if (bp >= &buf[sizeof buf])
		{
			/* send chunk */
			response = milter_send_command(m, SMFIC_BODY, buf,
						       bp - buf, e, state);
			bp = buf;
			if (bufchar != '\0')
			{
				*bp++ = bufchar;
				bufchar = '\0';
				prevchar = bufchar;
			}
		}
		if (m->mf_state == SMFS_ERROR ||
		    m->mf_state == SMFS_DONE ||
		    *state != SMFIR_CONTINUE)
			break;
	}

	/* check for read errors */
	if (ferror(e->e_dfp))
	{
		ExitStat = EX_IOERR;
		if (*state == SMFIR_CONTINUE ||
		    *state == SMFIR_ACCEPT)
		{
			*state = SMFIR_TEMPFAIL;
			if (response != NULL)
			{
				free(response);
				response = NULL;
			}
		}
		syserr("milter_body: %s/df%s: read error",
		       qid_printqueue(e->e_queuedir), e->e_id);
		return response;
	}

	/* send last body chunk */
	if (bp > buf &&
	    m->mf_state != SMFS_ERROR &&
	    m->mf_state != SMFS_DONE &&
	    *state == SMFIR_CONTINUE)
	{
		/* send chunk */
		response = milter_send_command(m, SMFIC_BODY, buf, bp - buf,
					       e, state);
		bp = buf;
	}
	return response;
}

/*
**  Actions
*/

/*
**  MILTER_ADDHEADER -- Add the supplied header to the message
**
**	Parameters:
**		response -- encoded form of header/value.
**		rlen -- length of response.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_addheader(response, rlen, e)
	char *response;
	ssize_t rlen;
	ENVELOPE *e;
{
	char *val;

	if (tTd(64, 10))
		dprintf("milter_addheader: ");

	/* sanity checks */
	if (response == NULL)
	{
		if (tTd(64, 10))
			dprintf("NULL response\n");
		return;
	}

	if (rlen < 2 || strlen(response) + 1 >= (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (total len)\n");
		return;
	}

	/* Find separating NUL */
	val = response + strlen(response) + 1;

	/* another sanity check */
	if (strlen(response) + strlen(val) + 2 != (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (part len)\n");
		return;
	}

	if (*response == '\0')
	{
		if (tTd(64, 10))
			dprintf("empty field name\n");
		return;
	}

	/* add to e_msgsize */
	e->e_msgsize += strlen(response) + 2 + strlen(val);

	if (tTd(64, 10))
		dprintf("Add %s: %s\n", response, val);

	addheader(newstr(response), val, H_USER, &e->e_header);
}
/*
**  MILTER_CHANGEHEADER -- Change the supplied header in the message
**
**	Parameters:
**		response -- encoded form of header/index/value.
**		rlen -- length of response.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_changeheader(response, rlen, e)
	char *response;
	ssize_t rlen;
	ENVELOPE *e;
{
	mi_int32 i, index;
	char *field, *val;
	HDR *h;

	if (tTd(64, 10))
		dprintf("milter_changeheader: ");

	/* sanity checks */
	if (response == NULL)
	{
		if (tTd(64, 10))
			dprintf("NULL response\n");
		return;
	}

	if (rlen < 2 || strlen(response) + 1 >= (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (total len)\n");
		return;
	}

	/* Find separating NUL */
	(void) memcpy((char *) &i, response, MILTER_LEN_BYTES);
	index = ntohl(i);
	field = response + MILTER_LEN_BYTES;
	val = field + strlen(field) + 1;

	/* another sanity check */
	if (MILTER_LEN_BYTES + strlen(field) + 1 +
	    strlen(val) + 1 != (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (part len)\n");
		return;
	}

	if (*field == '\0')
	{
		if (tTd(64, 10))
			dprintf("empty field name\n");
		return;
	}

	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		if (bitset(H_USER, h->h_flags) &&
		    strcasecmp(h->h_field, field) == 0 &&
		    --index <= 0)
			break;
	}

	if (h == NULL)
	{
		if (*val == '\0')
		{
			if (tTd(64, 10))
				dprintf("Delete (noop) %s:\n", field);
		}
		else
		{
			/* treat modify value with no existing header as add */
			if (tTd(64, 10))
				dprintf("Add %s: %s\n",	field, val);

			addheader(newstr(field), val, H_USER, &e->e_header);
		}
		return;
	}

	if (tTd(64, 10))
	{
		if (*val == '\0')
		{
			dprintf("Delete %s: %s\n", field,
				h->h_value == NULL ? "<NULL>" : h->h_value);
		}
		else
		{
			dprintf("Change %s: from %s to %s\n",
				field,
				h->h_value == NULL ? "<NULL>" : h->h_value,
				val);
		}
	}

	if (h->h_value != NULL)
	{
		e->e_msgsize -= strlen(h->h_value);
		free(h->h_value);
	}

	if (*val == '\0')
	{
		/* Remove "Field: " from message size */
		e->e_msgsize -= strlen(h->h_field) + 2;
		h->h_value = NULL;
	}
	else
	{
		h->h_value = newstr(val);
		e->e_msgsize += strlen(h->h_value);
	}
}
/*
**  MILTER_ADDRCPT -- Add the supplied recipient to the message
**
**	Parameters:
**		response -- encoded form of recipient address.
**		rlen -- length of response.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_addrcpt(response, rlen, e)
	char *response;
	ssize_t rlen;
	ENVELOPE *e;
{
	if (tTd(64, 10))
		dprintf("milter_addrcpt: ");

	/* sanity checks */
	if (response == NULL)
	{
		if (tTd(64, 10))
			dprintf("NULL response\n");
		return;
	}

	if (*response == '\0' ||
	    strlen(response) + 1 != (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (total len %d != rlen %d)\n",
				strlen(response), rlen -1);
		return;
	}

	if (tTd(64, 10))
		dprintf("%s\n", response);
	(void) sendtolist(response, NULLADDR, &e->e_sendqueue, 0, e);
	return;
}
/*
**  MILTER_DELRCPT -- Delete the supplied recipient from the message
**
**	Parameters:
**		response -- encoded form of recipient address.
**		rlen -- length of response.
**		e -- current envelope.
**
**	Returns:
**		none
*/

static void
milter_delrcpt(response, rlen, e)
	char *response;
	ssize_t rlen;
	ENVELOPE *e;
{
	if (tTd(64, 10))
		dprintf("milter_delrcpt: ");

	/* sanity checks */
	if (response == NULL)
	{
		if (tTd(64, 10))
			dprintf("NULL response\n");
		return;
	}

	if (*response == '\0' ||
	    strlen(response) + 1 != (size_t) rlen)
	{
		if (tTd(64, 10))
			dprintf("didn't follow protocol (total len)\n");
		return;
	}

	if (tTd(64, 10))
		dprintf("%s\n", response);
	(void) removefromlist(response, &e->e_sendqueue, e);
	return;
}
/*
**  MILTER_REPLBODY -- Replace the current df file with new body
**
**	Parameters:
**		response -- encoded form of new body.
**		rlen -- length of response.
**		newfilter -- if first time called by a new filter
**		e -- current envelope.
**
**	Returns:
**		0 upon success, -1 upon failure
*/

static int
milter_replbody(response, rlen, newfilter, e)
	char *response;
	ssize_t rlen;
	bool newfilter;
	ENVELOPE *e;
{
	static char prevchar;
	int i;

	if (tTd(64, 10))
		dprintf("milter_replbody\n");

	/* If a new filter, reset previous character and truncate df */
	if (newfilter)
	{
		off_t prevsize = 0;
		char dfname[MAXPATHLEN];

		(void) strlcpy(dfname, queuename(e, 'd'), sizeof dfname);

		/* Reset prevchar */
		prevchar = '\0';

		/* Get the current df information */
		if (bitset(EF_HAS_DF, e->e_flags) && e->e_dfp != NULL)
		{
			int afd;
			struct stat st;

			afd = fileno(e->e_dfp);
			if (afd > 0 && fstat(afd, &st) == 0)
				prevsize = st.st_size;
		}

		/* truncate current df file */
		if (bftruncate(e->e_dfp) < 0)
		{
			MILTER_DF_ERROR("milter_reopen_df: bftruncate %s: %s");
			return -1;
		}
		else
		{
			if (prevsize > e->e_msgsize)
				e->e_msgsize = 0;
			else
				e->e_msgsize -= prevsize;
		}
	}

	if (response == NULL)
	{
		/* Flush the buffered '\r' */
		if (prevchar == '\r')
		{
			(void) putc(prevchar, e->e_dfp);
			e->e_msgsize++;
		}
		return 0;
	}

	for (i = 0; i < rlen; i++)
	{
		/* Buffered char from last chunk */
		if (i == 0 && prevchar == '\r')
		{
			/* Not CRLF, output prevchar */
			if (response[i] != '\n')
			{
				(void) putc(prevchar, e->e_dfp);
				e->e_msgsize++;
			}
			prevchar = '\0';
		}

		/* Turn CRLF into LF */
		if (response[i] == '\r')
		{
			/* check if at end of chunk */
			if (i + 1 < rlen)
			{
				/* If LF, strip CR */
				if (response[i + 1] == '\n')
					i++;
			}
			else
			{
				/* check next chunk */
				prevchar = '\r';
				continue;
			}
		}
		(void) putc(response[i], e->e_dfp);
		e->e_msgsize++;
	}
	return 0;
}

/*
**  MTA callouts
*/

/*
**  MILTER_INIT -- open and negotiate with all of the filters
**
**	Parameters:
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		none
*/

/* ARGSUSED */
void
milter_init(e, state)
	ENVELOPE *e;
	char *state;
{
	int i;

	if (tTd(64, 10))
		dprintf("milter_init\n");

	*state = SMFIR_CONTINUE;
	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		m->mf_sock = milter_open(m, FALSE, e);
		if (m->mf_state == SMFS_ERROR)
		{
			MILTER_CHECK_ERROR(continue);
			break;
		}

		if (m->mf_sock < 0 ||
		    milter_negotiate(m, e) < 0 ||
		    m->mf_state == SMFS_ERROR)
		{
			if (tTd(64, 5))
				dprintf("milter_init(%s): failed to %s\n",
					m->mf_name,
					m->mf_sock < 0 ? "open" : "negotiate");

			/* if negotation failure, close socket */
			if (m->mf_sock >= 0)
			{
				(void) close(m->mf_sock);
				m->mf_sock = -1;
			}
			milter_error(m);
			if (m->mf_state == SMFS_ERROR)
			{
				MILTER_CHECK_ERROR(continue);
				break;
			}
		}
	}

	/*
	**  If something temp/perm failed with one of the filters,
	**  we won't be using any of them, so clear any existing
	**  connections.
	*/

	if (*state != SMFIR_CONTINUE)
		milter_quit(e);
}
/*
**  MILTER_CONNECT -- send connection info to milter filters
**
**	Parameters:
**		hostname -- hostname of remote machine.
**		addr -- address of remote machine.
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

char *
milter_connect(hostname, addr, e, state)
	char *hostname;
	SOCKADDR addr;
	ENVELOPE *e;
	char *state;
{
	char family;
	u_short port;
	char *buf, *bp;
	char *response;
	char *sockinfo = NULL;
	ssize_t s;
# if NETINET6
	char buf6[INET6_ADDRSTRLEN];
# endif /* NETINET6 */

	if (tTd(64, 10))
		dprintf("milter_connect(%s)\n", hostname);

	/* gather data */
	switch (addr.sa.sa_family)
	{
# if NETUNIX
	  case AF_UNIX:
		family = SMFIA_UNIX;
		port = htons(0);
		sockinfo = addr.sunix.sun_path;
		break;
# endif /* NETUNIX */

# if NETINET
	  case AF_INET:
		family = SMFIA_INET;
		port = htons(addr.sin.sin_port);
		sockinfo = (char *) inet_ntoa(addr.sin.sin_addr);
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		family = SMFIA_INET6;
		port = htons(addr.sin6.sin6_port);
		sockinfo = anynet_ntop(&addr.sin6.sin6_addr, buf6,
				       sizeof buf6);
		if (sockinfo == NULL)
			sockinfo = "";
		break;
# endif /* NETINET6 */

	  default:
		family = SMFIA_UNKNOWN;
		break;
	}

	s = strlen(hostname) + 1 + sizeof(family);
	if (family != SMFIA_UNKNOWN)
		s += sizeof(port) + strlen(sockinfo) + 1;

	buf = (char *)xalloc(s);
	bp = buf;

	/* put together data */
	(void) memcpy(bp, hostname, strlen(hostname));
	bp += strlen(hostname);
	*bp++ = '\0';
	(void) memcpy(bp, &family, sizeof family);
	bp += sizeof family;
	if (family != SMFIA_UNKNOWN)
	{
		(void) memcpy(bp, &port, sizeof port);
		bp += sizeof port;

		/* include trailing '\0' */
		(void) memcpy(bp, sockinfo, strlen(sockinfo) + 1);
	}

	response = milter_command(SMFIC_CONNECT, buf, s,
				  MilterConnectMacros, e, state);
	free(buf);

	/*
	**  If this message connection is done for,
	**  close the filters.
	*/

	if (*state != SMFIR_CONTINUE)
		milter_quit(e);
	else
		milter_per_connection_check(e);

	/*
	**  SMFIR_REPLYCODE can't work with connect due to
	**  the requirements of SMTP.  Therefore, ignore the
	**  reply code text but keep the state it would reflect.
	*/

	if (*state == SMFIR_REPLYCODE)
	{
		if (response != NULL &&
		    *response == '4')
			*state = SMFIR_TEMPFAIL;
		else
			*state = SMFIR_REJECT;
		if (response != NULL)
		{
			free(response);
			response = NULL;
		}
	}
	return response;
}
/*
**  MILTER_HELO -- send SMTP HELO/EHLO command info to milter filters
**
**	Parameters:
**		helo -- argument to SMTP HELO/EHLO command.
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

char *
milter_helo(helo, e, state)
	char *helo;
	ENVELOPE *e;
	char *state;
{
	char *response;

	if (tTd(64, 10))
		dprintf("milter_helo(%s)\n", helo);

	response = milter_command(SMFIC_HELO, helo, strlen(helo) + 1,
				  MilterHeloMacros, e, state);
	milter_per_connection_check(e);
	return response;
}
/*
**  MILTER_ENVFROM -- send SMTP MAIL command info to milter filters
**
**	Parameters:
**		args -- SMTP MAIL command args (args[0] == sender).
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

char *
milter_envfrom(args, e, state)
	char **args;
	ENVELOPE *e;
	char *state;
{
	int i;
	char *buf, *bp;
	char *response;
	ssize_t s;

	if (tTd(64, 10))
	{
		dprintf("milter_envfrom:");
		for (i = 0; args[i] != NULL; i++)
			dprintf(" %s", args[i]);
		dprintf("\n");
	}

	/* sanity check */
	if (args[0] == NULL)
	{
		*state = SMFIR_REJECT;
		return NULL;
	}

	/* new message, so ... */
	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		switch (m->mf_state)
		{
		  case SMFS_INMSG:
			/* abort in message filters */
			milter_abort_filter(m, e);
			/* FALLTHROUGH */

		  case SMFS_DONE:
			/* reset done filters */
			m->mf_state = SMFS_OPEN;
			break;
		}
	}

	/* put together data */
	s = 0;
	for (i = 0; args[i] != NULL; i++)
		s += strlen(args[i]) + 1;
	buf = (char *)xalloc(s);
	bp = buf;
	for (i = 0; args[i] != NULL; i++)
	{
		(void) strlcpy(bp, args[i], s - (bp - buf));
		bp += strlen(bp) + 1;
	}

	/* send it over */
	response = milter_command(SMFIC_MAIL, buf, s,
				  MilterEnvFromMacros, e, state);
	free(buf);

	/*
	**  If filter rejects/discards a per message command,
	**  abort the other filters since we are done with the
	**  current message.
	*/

	MILTER_CHECK_DONE_MSG();
	return response;
}
/*
**  MILTER_ENVRCPT -- send SMTP RCPT command info to milter filters
**
**	Parameters:
**		args -- SMTP MAIL command args (args[0] == recipient).
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
*/

char *
milter_envrcpt(args, e, state)
	char **args;
	ENVELOPE *e;
	char *state;
{
	int i;
	char *buf, *bp;
	char *response;
	ssize_t s;

	if (tTd(64, 10))
	{
		dprintf("milter_envrcpt:");
		for (i = 0; args[i] != NULL; i++)
			dprintf(" %s", args[i]);
		dprintf("\n");
	}

	/* sanity check */
	if (args[0] == NULL)
	{
		*state = SMFIR_REJECT;
		return NULL;
	}

	/* put together data */
	s = 0;
	for (i = 0; args[i] != NULL; i++)
		s += strlen(args[i]) + 1;
	buf = (char *)xalloc(s);
	bp = buf;
	for (i = 0; args[i] != NULL; i++)
	{
		(void) strlcpy(bp, args[i], s - (bp - buf));
		bp += strlen(bp) + 1;
	}

	/* send it over */
	response = milter_command(SMFIC_RCPT, buf, s,
				  MilterEnvRcptMacros, e, state);
	free(buf);
	return response;
}
/*
**  MILTER_DATA -- send message headers/body and gather final message results
**
**	Parameters:
**		e -- current envelope.
**		state -- return state from response.
**
**	Returns:
**		response string (may be NULL)
**
**	Side effects:
**		- Uses e->e_dfp for access to the body
**		- Can call the various milter action routines to
**		  modify the envelope or message.
*/

# define MILTER_CHECK_RESULTS() \
	if (*state == SMFIR_ACCEPT || \
	    m->mf_state == SMFS_DONE || \
	    m->mf_state == SMFS_ERROR) \
	{ \
		if (m->mf_state != SMFS_ERROR) \
			m->mf_state = SMFS_DONE; \
		continue;	/* to next filter */ \
	} \
	if (*state != SMFIR_CONTINUE) \
	{ \
		m->mf_state = SMFS_DONE; \
		goto finishup; \
	}

char *
milter_data(e, state)
	ENVELOPE *e;
	char *state;
{
	bool replbody = FALSE;		/* milter_replbody() called? */
	bool replfailed = FALSE;	/* milter_replbody() failed? */
	bool rewind = FALSE;		/* rewind df file? */
	bool dfopen = FALSE;		/* df open for writing? */
	bool newfilter;			/* reset on each new filter */
	char rcmd;
	int i;
	int save_errno;
	char *response = NULL;
	time_t eomsent;
	ssize_t rlen;

	if (tTd(64, 10))
		dprintf("milter_data\n");

	*state = SMFIR_CONTINUE;

	/*
	**  XXX: Should actually send body chunks to each filter
	**  a chunk at a time instead of sending the whole body to
	**  each filter in turn.  However, only if the filters don't
	**  change the body.
	*/

	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		if (*state != SMFIR_CONTINUE &&
		    *state != SMFIR_ACCEPT)
		{
			/*
			**  A previous filter has dealt with the message,
			**  safe to stop processing the filters.
			*/

			break;
		}

		/* Now reset state for later evaluation */
		*state = SMFIR_CONTINUE;
		newfilter = TRUE;

		/* sanity checks */
		if (m->mf_sock < 0 ||
		    (m->mf_state != SMFS_OPEN && m->mf_state != SMFS_INMSG))
			continue;

		m->mf_state = SMFS_INMSG;

		/* check if filter wants the headers */
		if (!bitset(SMFIP_NOHDRS, m->mf_pflags))
		{
			response = milter_headers(m, e, state);
			MILTER_CHECK_RESULTS();
		}

		/* check if filter wants EOH */
		if (!bitset(SMFIP_NOEOH, m->mf_pflags))
		{
			if (tTd(64, 10))
				dprintf("milter_data: eoh\n");

			/* send it over */
			response = milter_send_command(m, SMFIC_EOH, NULL, 0,
						       e, state);
			MILTER_CHECK_RESULTS();
		}

		/* check if filter wants the body */
		if (!bitset(SMFIP_NOBODY, m->mf_pflags) &&
		    e->e_dfp != NULL)
		{
			rewind = TRUE;
			response = milter_body(m, e, state);
			MILTER_CHECK_RESULTS();
		}

		/* send the final body chunk */
		(void) milter_write(m, SMFIC_BODYEOB, NULL, 0,
				    m->mf_timeout[SMFTO_WRITE], e);

		/* Get time EOM sent for timeout */
		eomsent = curtime();

		/* deal with the possibility of multiple responses */
		while (*state == SMFIR_CONTINUE)
		{
			/* Check total timeout from EOM to final ACK/NAK */
			if (m->mf_timeout[SMFTO_EOM] > 0 &&
			    curtime() - eomsent >= m->mf_timeout[SMFTO_EOM])
			{
				if (tTd(64, 5))
					dprintf("milter_data(%s): EOM ACK/NAK timeout\n",
						m->mf_name);
				if (LogLevel > 0)
					sm_syslog(LOG_ERR, e->e_id,
						  "milter_data(%s): EOM ACK/NAK timeout\n",
						  m->mf_name);
				milter_error(m);
				MILTER_CHECK_ERROR(continue);
				break;
			}

			response = milter_read(m, &rcmd, &rlen,
					       m->mf_timeout[SMFTO_READ], e);
			if (m->mf_state == SMFS_ERROR)
				break;

			if (tTd(64, 10))
				dprintf("milter_data(%s): state %c\n",
					m->mf_name, (char) rcmd);

			switch (rcmd)
			{
			  case SMFIR_REPLYCODE:
				MILTER_CHECK_REPLYCODE("554 5.7.1 Command rejected");
				*state = rcmd;
				m->mf_state = SMFS_DONE;
				break;

			  case SMFIR_REJECT:
			  case SMFIR_DISCARD:
			  case SMFIR_TEMPFAIL:
				*state = rcmd;
				m->mf_state = SMFS_DONE;
				break;

			  case SMFIR_CONTINUE:
			  case SMFIR_ACCEPT:
				/* this filter is done with message */
				if (replfailed)
					*state = SMFIR_TEMPFAIL;
				else
					*state = SMFIR_ACCEPT;
				m->mf_state = SMFS_DONE;
				break;

			  case SMFIR_PROGRESS:
				break;

			  case SMFIR_ADDHEADER:
				if (!bitset(SMFIF_ADDHDRS, m->mf_fflags))
				{
					if (LogLevel > 9)
						sm_syslog(LOG_WARNING, e->e_id,
							  "milter_data(%s): lied about adding headers, honoring request anyway",
							  m->mf_name);
				}
				milter_addheader(response, rlen, e);
				break;

			  case SMFIR_CHGHEADER:
				if (!bitset(SMFIF_CHGHDRS, m->mf_fflags))
				{
					if (LogLevel > 9)
						sm_syslog(LOG_WARNING, e->e_id,
							  "milter_data(%s): lied about changing headers, honoring request anyway",
							  m->mf_name);
				}
				milter_changeheader(response, rlen, e);
				break;

			  case SMFIR_ADDRCPT:
				if (!bitset(SMFIF_ADDRCPT, m->mf_fflags))
				{
					if (LogLevel > 9)
						sm_syslog(LOG_WARNING, e->e_id,
							  "milter_data(%s) lied about adding recipients, honoring request anyway",
							  m->mf_name);
				}
				milter_addrcpt(response, rlen, e);
				break;

			  case SMFIR_DELRCPT:
				if (!bitset(SMFIF_DELRCPT, m->mf_fflags))
				{
					if (LogLevel > 9)
						sm_syslog(LOG_WARNING, e->e_id,
							  "milter_data(%s): lied about removing recipients, honoring request anyway",
							  m->mf_name);
				}
				milter_delrcpt(response, rlen, e);
				break;

			  case SMFIR_REPLBODY:
				if (!bitset(SMFIF_MODBODY, m->mf_fflags))
				{
					if (LogLevel > 0)
						sm_syslog(LOG_ERR, e->e_id,
							  "milter_data(%s): lied about replacing body, rejecting request and tempfailing message",
							  m->mf_name);
					replfailed = TRUE;
					break;
				}

				/* already failed in attempt */
				if (replfailed)
					break;

				if (!dfopen)
				{
					if (milter_reopen_df(e) < 0)
					{
						replfailed = TRUE;
						break;
					}
					dfopen = TRUE;
					rewind = TRUE;
				}

				if (milter_replbody(response, rlen,
						    newfilter, e) < 0)
					replfailed = TRUE;
				newfilter = FALSE;
				replbody = TRUE;
				break;

			  default:
				/* Invalid response to command */
				if (LogLevel > 0)
					sm_syslog(LOG_ERR, e->e_id,
						  "milter_data(%s): returned bogus response %c",
						  m->mf_name, rcmd);
				milter_error(m);
				break;
			}
			if (rcmd != SMFIR_REPLYCODE &&
			    response != NULL)
			{
				free(response);
				response = NULL;
			}

			if (m->mf_state == SMFS_ERROR)
				break;
		}

		if (replbody && !replfailed)
		{
			/* flush possible buffered character */
			milter_replbody(NULL, 0, !replbody, e);
			replbody = FALSE;
		}

		if (m->mf_state == SMFS_ERROR)
		{
			MILTER_CHECK_ERROR(continue);
			goto finishup;
		}
	}

finishup:
	/* leave things in the expected state if we touched it */
	if (replfailed)
	{
		if (*state == SMFIR_CONTINUE ||
		    *state == SMFIR_ACCEPT)
		{
			*state = SMFIR_TEMPFAIL;
			if (response != NULL)
			{
				free(response);
				response = NULL;
			}
		}

		if (dfopen)
		{
			(void) fclose(e->e_dfp);
			e->e_dfp = NULL;
			e->e_flags &= ~EF_HAS_DF;
			dfopen = FALSE;
		}
		rewind = FALSE;
	}

	if ((dfopen && milter_reset_df(e) < 0) ||
	    (rewind && bfrewind(e->e_dfp) < 0))
	{
		save_errno = errno;
		ExitStat = EX_IOERR;

		/*
		**  If filter told us to keep message but we had
		**  an error, we can't really keep it, tempfail it.
		*/

		if (*state == SMFIR_CONTINUE ||
		    *state == SMFIR_ACCEPT)
		{
			*state = SMFIR_TEMPFAIL;
			if (response != NULL)
			{
				free(response);
				response = NULL;
			}
		}

		errno = save_errno;
		syserr("milter_data: %s/df%s: read error",
		       qid_printqueue(e->e_queuedir), e->e_id);
	}
	MILTER_CHECK_DONE_MSG();
	return response;
}
/*
**  MILTER_QUIT -- informs the filter(s) we are done and closes connection(s)
**
**	Parameters:
**		e -- current envelope.
**
**	Returns:
**		none
*/

void
milter_quit(e)
	ENVELOPE *e;
{
	int i;

	if (tTd(64, 10))
		dprintf("milter_quit\n");

	for (i = 0; InputFilters[i] != NULL; i++)
		milter_quit_filter(InputFilters[i], e);
}
/*
**  MILTER_ABORT -- informs the filter(s) that we are aborting current message
**
**	Parameters:
**		e -- current envelope.
**
**	Returns:
**		none
*/

void
milter_abort(e)
	ENVELOPE *e;
{
	int i;

	if (tTd(64, 10))
		dprintf("milter_abort\n");

	for (i = 0; InputFilters[i] != NULL; i++)
	{
		struct milter *m = InputFilters[i];

		/* sanity checks */
		if (m->mf_sock < 0 || m->mf_state != SMFS_INMSG)
			continue;

		milter_abort_filter(m, e);
	}
}
#endif /* _FFR_MILTER */
