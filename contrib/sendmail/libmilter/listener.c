/*
 *  Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: listener.c,v 8.38.2.1.2.11 2000/09/01 00:49:04 ca Exp $";
#endif /* ! lint */

#if _FFR_MILTER
/*
**  listener.c -- threaded network listener
*/

#include "libmilter.h"


# if NETINET || NETINET6
#  include <arpa/inet.h>
# endif /* NETINET || NETINET6 */
/*
**  MI_MILTEROPEN -- setup socket to listen on
**
**	Parameters:
**		conn -- connection description
**		backlog -- listen backlog
**		socksize -- socksize of created socket
**
**	Returns:
**		socket upon success, error code otherwise.
*/

static socket_t
mi_milteropen(conn, backlog, socksize, name)
	char *conn;
	int backlog;
	SOCKADDR_LEN_T *socksize;
	char *name;
{
	socket_t sock;
	int sockopt = 1;
	char *p;
	char *colon;
	char *at;
	struct hostent *hp = NULL;
	SOCKADDR addr;

	if (conn == NULL || conn[0] == '\0')
	{
		smi_log(SMI_LOG_ERR, "%s: empty or missing socket information",
			name);
		return INVALID_SOCKET;
	}
	(void) memset(&addr, '\0', sizeof addr);

	/* protocol:filename or protocol:port@host */
	p = conn;
	colon = strchr(p, ':');
	if (colon != NULL)
	{
		*colon = '\0';

 		if (*p == '\0')
		{
#if NETUNIX
			/* default to AF_UNIX */
 			addr.sa.sa_family = AF_UNIX;
			*socksize = sizeof (struct sockaddr_un);
#else /* NETUNIX */
# if NETINET
			/* default to AF_INET */
			addr.sa.sa_family = AF_INET;
			*socksize = sizeof addr.sin;
# else /* NETINET */
#  if NETINET6
			/* default to AF_INET6 */
			addr.sa.sa_family = AF_INET6;
			*socksize = sizeof addr.sin6;
#  else /* NETINET6 */
			/* no protocols available */
			smi_log(SMI_LOG_ERR,
				"%s: no valid socket protocols available",
				name);
			return INVALID_SOCKET;
#  endif /* NETINET6 */
# endif /* NETINET */
#endif /* NETUNIX */
		}
#if NETUNIX
		else if (strcasecmp(p, "unix") == 0 ||
			 strcasecmp(p, "local") == 0)
		{
			addr.sa.sa_family = AF_UNIX;
			*socksize = sizeof (struct sockaddr_un);
		}
#endif /* NETUNIX */
#if NETINET
		else if (strcasecmp(p, "inet") == 0)
		{
			addr.sa.sa_family = AF_INET;
			*socksize = sizeof addr.sin;
		}
#endif /* NETINET */
#if NETINET6
		else if (strcasecmp(p, "inet6") == 0)
		{
			addr.sa.sa_family = AF_INET6;
			*socksize = sizeof addr.sin6;
		}
#endif /* NETINET6 */
		else
		{
			smi_log(SMI_LOG_ERR, "%s: unknown socket type %s",
				name, p);
			return INVALID_SOCKET;
		}
		*colon++ = ':';
	}
	else
	{
		colon = p;
#if NETUNIX
		/* default to AF_UNIX */
 		addr.sa.sa_family = AF_UNIX;
		*socksize = sizeof (struct sockaddr_un);
#else /* NETUNIX */
# if NETINET
		/* default to AF_INET */
		addr.sa.sa_family = AF_INET;
		*socksize = sizeof addr.sin;
# else /* NETINET */
#  if NETINET6
		/* default to AF_INET6 */
		addr.sa.sa_family = AF_INET6;
		*socksize = sizeof addr.sin6;
#  else /* NETINET6 */
		smi_log(SMI_LOG_ERR, "%s: unknown socket type %s",
			name, p);
		return INVALID_SOCKET;
#  endif /* NETINET6 */
# endif /* NETINET */
#endif /* NETUNIX */
	}

#if NETUNIX
	if (addr.sa.sa_family == AF_UNIX)
	{
# if 0
		long sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_CREAT|SFF_MUSTOWN;
# endif /* 0 */

		at = colon;
		if (strlcpy(addr.sunix.sun_path, colon,
			    sizeof addr.sunix.sun_path) >=
		    sizeof addr.sunix.sun_path)
		{
			errno = EINVAL;
			smi_log(SMI_LOG_ERR, "%s: UNIX socket name %s too long",
				name, colon);
			return INVALID_SOCKET;
		}
# if 0
		errno = safefile(colon, RunAsUid, RunAsGid, RunAsUserName, sff,
				 S_IRUSR|S_IWUSR, NULL);

		/* if not safe, don't create */
		if (errno != 0)
		{
			smi_log(SMI_LOG_ERR,
				"%s: UNIX socket name %s unsafe",
				name, colon);
			return INVALID_SOCKET;
		}
# endif /* 0 */

	}
#endif /* NETUNIX */

#if NETINET || NETINET6
	if (
# if NETINET
	    addr.sa.sa_family == AF_INET
# endif /* NETINET */
# if NETINET && NETINET6
	    ||
# endif /* NETINET && NETINET6 */
# if NETINET6
	    addr.sa.sa_family == AF_INET6
# endif /* NETINET6 */
	   )
	{
		u_short port;

		/* Parse port@host */
		at = strchr(colon, '@');
		if (at == NULL)
		{
			switch (addr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				addr.sin.sin_addr.s_addr = INADDR_ANY;
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				addr.sin6.sin6_addr = in6addr_any;
				break;
# endif /* NETINET6 */
			}
		}
		else
			*at = '\0';

		if (isascii(*colon) && isdigit(*colon))
			port = htons((u_short) atoi(colon));
		else
		{
# ifdef NO_GETSERVBYNAME
			smi_log(SMI_LOG_ERR, "%s: invalid port number %s",
				name, colon);
			return INVALID_SOCKET;
# else /* NO_GETSERVBYNAME */
			register struct servent *sp;

			sp = getservbyname(colon, "tcp");
			if (sp == NULL)
			{
				smi_log(SMI_LOG_ERR,
					"%s: unknown port name %s",
					name, colon);
				return INVALID_SOCKET;
			}
			port = sp->s_port;
# endif /* NO_GETSERVBYNAME */
		}
		if (at != NULL)
		{
			*at++ = '@';
			if (*at == '[')
			{
				char *end;

				end = strchr(at, ']');
				if (end != NULL)
				{
					bool found = FALSE;
# if NETINET
					unsigned long hid = INADDR_NONE;
# endif /* NETINET */
# if NETINET6
					struct sockaddr_in6 hid6;
# endif /* NETINET6 */

					*end = '\0';
# if NETINET
					if (addr.sa.sa_family == AF_INET &&
					    (hid = inet_addr(&at[1])) !=
					    INADDR_NONE)
					{
						addr.sin.sin_addr.s_addr = hid;
						addr.sin.sin_port = port;
						found = TRUE;
					}
# endif /* NETINET */
# if NETINET6
					(void) memset(&hid6, '\0', sizeof hid6);
					if (addr.sa.sa_family == AF_INET6 &&
					    inet_pton(AF_INET6, &at[1],
						      &hid6.sin6_addr) == 1)
					{
						addr.sin6.sin6_addr = hid6.sin6_addr;
						addr.sin6.sin6_port = port;
						found = TRUE;
					}
# endif /* NETINET6 */
					*end = ']';
					if (!found)
					{
						smi_log(SMI_LOG_ERR,
							"%s: Invalid numeric domain spec \"%s\"",
							name, at);
						return INVALID_SOCKET;
					}
				}
				else
				{
					smi_log(SMI_LOG_ERR,
						"%s: Invalid numeric domain spec \"%s\"",
						name, at);
					return INVALID_SOCKET;
				}
			}
			else
			{
				hp = mi_gethostbyname(at, addr.sa.sa_family);
				if (hp == NULL)
				{
					smi_log(SMI_LOG_ERR,
						"%s: Unknown host name %s",
						name, at);
					return INVALID_SOCKET;
				}
				addr.sa.sa_family = hp->h_addrtype;
				switch (hp->h_addrtype)
				{
# if NETINET
				  case AF_INET:
					memmove(&addr.sin.sin_addr,
						hp->h_addr,
						INADDRSZ);
					addr.sin.sin_port = port;
					break;
# endif /* NETINET */

# if NETINET6
				  case AF_INET6:
					memmove(&addr.sin6.sin6_addr,
						hp->h_addr,
						IN6ADDRSZ);
					addr.sin6.sin6_port = port;
					break;
# endif /* NETINET6 */

				  default:
					smi_log(SMI_LOG_ERR,
						"%s: Unknown protocol for %s (%d)",
						name, at, hp->h_addrtype);
					return INVALID_SOCKET;
				}
			}
		}
		else
		{
			switch (addr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				addr.sin.sin_port = port;
				break;
# endif /* NETINET */
# if NETINET6
			  case AF_INET6:
				addr.sin6.sin6_port = port;
				break;
# endif /* NETINET6 */
			}
		}
	}
#endif /* NETINET || NETINET6 */

	sock = socket(addr.sa.sa_family, SOCK_STREAM, 0);
	if (!ValidSocket(sock))
	{
		smi_log(SMI_LOG_ERR,
			"%s: Unable to create new socket: %s",
			name, strerror(errno));
		return INVALID_SOCKET;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &sockopt,
		       sizeof(sockopt)) == -1)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Unable to setsockopt: %s", name, strerror(errno));
		(void) close(sock);
		return INVALID_SOCKET;
	}

	if (bind(sock, &addr.sa, *socksize) < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Unable to bind to port %s: %s",
			name, conn, strerror(errno));
		(void) close(sock);
		return INVALID_SOCKET;
	}

	if (listen(sock, backlog) < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: listen call failed: %s", name, strerror(errno));
		(void) close(sock);
		return INVALID_SOCKET;
	}

	return sock;
}
/*
**  MI_THREAD_HANDLE_WRAPPER -- small wrapper to handle session
**
**	Parameters:
**		arg -- argument to pass to mi_handle_session()
**
**	Returns:
**		results from mi_handle_session()
*/

void *
mi_thread_handle_wrapper(arg)
	void *arg;
{
	return (void *) mi_handle_session(arg);
}

static socket_t listenfd = INVALID_SOCKET;

/*
**  MI_CLOSENER -- close listen socket
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
mi_closener()
{
	if (ValidSocket(listenfd))
	{
		(void) close(listenfd);
		listenfd = INVALID_SOCKET;
	}
}

/*
**  MI_LISTENER -- Generic listener harness
**
**	Open up listen port
**	Wait for connections
**
**	Parameters:
**		conn -- connection description
**		dbg -- debug level
**		smfi -- filter structure to use
**		timeout -- timeout for reads/writes
**
**	Returns:
**		MI_SUCCESS -- Exited normally
**			   (session finished or we were told to exit)
**		MI_FAILURE -- Network initialization failed.
*/

int
mi_listener(conn, dbg, smfi, timeout, backlog)
	char *conn;
	int dbg;
	smfiDesc_ptr smfi;
	time_t timeout;
	int backlog;
{
	socket_t connfd = INVALID_SOCKET;
	int sockopt = 1;
	int r;
	int ret = MI_SUCCESS;
	int cnt_m = 0;
	int cnt_t = 0;
	sthread_t thread_id;
	_SOCK_ADDR cliaddr;
	SOCKADDR_LEN_T socksize;
	SOCKADDR_LEN_T clilen;
	SMFICTX_PTR ctx;
	fd_set readset, excset;
	struct timeval chktime;

	if (dbg > 0)
		smi_log(SMI_LOG_DEBUG,
			"%s: Opening listen socket on conn %s",
			smfi->xxfi_name, conn);
	listenfd = mi_milteropen(conn, backlog, &socksize, smfi->xxfi_name);
	if (!ValidSocket(listenfd))
	{
		smi_log(SMI_LOG_FATAL,
			"%s: Unable to create listening socket on conn %s",
			smfi->xxfi_name, conn);
		return MI_FAILURE;
	}
	clilen = socksize;
	if (listenfd >= FD_SETSIZE)
	{
		smi_log(SMI_LOG_ERR, "%s: fd %d is larger than FD_SETSIZE %d",
			smfi->xxfi_name, listenfd, FD_SETSIZE);
		return MI_FAILURE;
	}

	while (mi_stop() == MILTER_CONT)
	{
		/* select on interface ports */
		FD_ZERO(&readset);
		FD_SET((u_int) listenfd, &readset);
		FD_ZERO(&excset);
		FD_SET((u_int) listenfd, &excset);
		chktime.tv_sec = MI_CHK_TIME;
		chktime.tv_usec = 0;
		r = select(listenfd + 1, &readset, NULL, &excset, &chktime);
		if (r == 0)		/* timeout */
			continue;	/* just check mi_stop() */
		if (r < 0)
		{
			if (errno == EINTR)
				continue;
			ret = MI_FAILURE;
			break;
		}
		if (!FD_ISSET(listenfd, &readset))
		{
			/* some error: just stop for now... */
			ret = MI_FAILURE;
			break;
		}

		connfd = accept(listenfd, (struct sockaddr *) &cliaddr,
				&clilen);

		if (!ValidSocket(connfd))
		{
			smi_log(SMI_LOG_ERR,
				"%s: accept() returned invalid socket",
				smfi->xxfi_name);
			continue;
		}

		if (setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE,
				(void *) &sockopt, sizeof sockopt) < 0)
		{
			smi_log(SMI_LOG_WARN, "%s: setsockopt() failed",
				smfi->xxfi_name);
			/* XXX: continue? */
		}
		if ((ctx = (SMFICTX_PTR) malloc(sizeof *ctx)) == NULL)
		{
			(void) close(connfd);
			smi_log(SMI_LOG_ERR, "%s: malloc(ctx) failed",
				smfi->xxfi_name);
			sleep(++cnt_m);
			if (cnt_m >= MAX_FAILS_M)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		cnt_m = 0;
		memset(ctx, '\0', sizeof *ctx);
		ctx->ctx_sd = connfd;
		ctx->ctx_dbg = dbg;
		ctx->ctx_timeout = timeout;
		ctx->ctx_smfi = smfi;
#if 0
		if (smfi->xxfi_eoh == NULL)
		if (smfi->xxfi_eom == NULL)
		if (smfi->xxfi_abort == NULL)
		if (smfi->xxfi_close == NULL)
#endif /* 0 */
		if (smfi->xxfi_connect == NULL)
			ctx->ctx_pflags |= SMFIP_NOCONNECT;
		if (smfi->xxfi_helo == NULL)
			ctx->ctx_pflags |= SMFIP_NOHELO;
		if (smfi->xxfi_envfrom == NULL)
			ctx->ctx_pflags |= SMFIP_NOMAIL;
		if (smfi->xxfi_envrcpt == NULL)
			ctx->ctx_pflags |= SMFIP_NORCPT;
		if (smfi->xxfi_header == NULL)
			ctx->ctx_pflags |= SMFIP_NOHDRS;
		if (smfi->xxfi_eoh == NULL)
			ctx->ctx_pflags |= SMFIP_NOEOH;
		if (smfi->xxfi_body == NULL)
			ctx->ctx_pflags |= SMFIP_NOBODY;

		if ((r = thread_create(&thread_id,
					mi_thread_handle_wrapper,
					(void *) ctx)) != MI_SUCCESS)
		{
			smi_log(SMI_LOG_ERR,
				"%s: thread_create() failed: %d",
				smfi->xxfi_name,  r);
			sleep(++cnt_t);
			(void) close(connfd);
			free(ctx);
			if (cnt_t >= MAX_FAILS_T)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		cnt_t = 0;
	}
	if (ret != MI_SUCCESS)
		mi_stop_milters(MILTER_ABRT);
	if (listenfd >= 0)
		(void) close(listenfd);
	return ret;
}
#endif /* _FFR_MILTER */
