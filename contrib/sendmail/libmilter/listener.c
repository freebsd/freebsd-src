/*
 *  Copyright (c) 1999-2002 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: listener.c,v 8.82 2002/01/22 18:46:47 ca Exp $")

/*
**  listener.c -- threaded network listener
*/

#include "libmilter.h"
#include <sm/errstring.h>


# if NETINET || NETINET6
#  include <arpa/inet.h>
# endif /* NETINET || NETINET6 */

static smutex_t L_Mutex;

/*
**  MI_MILTEROPEN -- setup socket to listen on
**
**	Parameters:
**		conn -- connection description
**		backlog -- listen backlog
**		socksize -- socksize of created socket
**		family -- family of created socket
**		name -- name for logging
**
**	Returns:
**		socket upon success, error code otherwise.
**
**	Side effect:
**		sets sockpath if UNIX socket.
*/

#if NETUNIX
static char	*sockpath = NULL;
#endif /* NETUNIX */

static socket_t
mi_milteropen(conn, backlog, socksize, family, name)
	char *conn;
	int backlog;
	SOCKADDR_LEN_T *socksize;
	int *family;
	char *name;
{
	socket_t sock;
	int sockopt = 1;
	size_t len = 0;
	char *p;
	char *colon;
	char *at;
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
		len = strlen(colon) + 1;
		if (len >= sizeof addr.sunix.sun_path)
		{
			errno = EINVAL;
			smi_log(SMI_LOG_ERR, "%s: UNIX socket name %s too long",
				name, colon);
			return INVALID_SOCKET;
		}
		(void) sm_strlcpy(addr.sunix.sun_path, colon,
				sizeof addr.sunix.sun_path);
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
		unsigned short port;

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
			port = htons((unsigned short) atoi(colon));
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
					bool found = false;
# if NETINET
					unsigned long hid = INADDR_NONE;
# endif /* NETINET */
# if NETINET6
					struct sockaddr_in6 hid6;
# endif /* NETINET6 */

					*end = '\0';
# if NETINET
					if (addr.sa.sa_family == AF_INET &&
					    (hid = inet_addr(&at[1])) != INADDR_NONE)
					{
						addr.sin.sin_addr.s_addr = hid;
						addr.sin.sin_port = port;
						found = true;
					}
# endif /* NETINET */
# if NETINET6
					(void) memset(&hid6, '\0', sizeof hid6);
					if (addr.sa.sa_family == AF_INET6 &&
					    mi_inet_pton(AF_INET6, &at[1],
							 &hid6.sin6_addr) == 1)
					{
						addr.sin6.sin6_addr = hid6.sin6_addr;
						addr.sin6.sin6_port = port;
						found = true;
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
				struct hostent *hp = NULL;

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
# if NETINET6
				freehostent(hp);
# endif /* NETINET6 */
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
			name, sm_errstring(errno));
		return INVALID_SOCKET;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &sockopt,
		       sizeof(sockopt)) == -1)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Unable to setsockopt: %s", name,
			sm_errstring(errno));
		(void) closesocket(sock);
		return INVALID_SOCKET;
	}

	if (bind(sock, &addr.sa, *socksize) < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Unable to bind to port %s: %s",
			name, conn, sm_errstring(errno));
		(void) closesocket(sock);
		return INVALID_SOCKET;
	}

	if (listen(sock, backlog) < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: listen call failed: %s", name,
			sm_errstring(errno));
		(void) closesocket(sock);
		return INVALID_SOCKET;
	}

#if NETUNIX
	if (addr.sa.sa_family == AF_UNIX && len > 0)
	{
		/*
		**  Set global variable sockpath so the UNIX socket can be
		**  unlink()ed at exit.
		*/

		sockpath = (char *) malloc(len);
		if (sockpath != NULL)
			(void) sm_strlcpy(sockpath, colon, len);
		else
		{
			smi_log(SMI_LOG_ERR,
				"%s: can't malloc(%d) for sockpath: %s",
				name, len, sm_errstring(errno));
			(void) closesocket(sock);
			return INVALID_SOCKET;
		}
	}
#endif /* NETUNIX */
	*family = addr.sa.sa_family;
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
**	NOTE: It is assumed that this function is called from a
**	      function that has a mutex lock (currently mi_stop_milters()).
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
	(void) smutex_lock(&L_Mutex);
	if (ValidSocket(listenfd))
	{
#if NETUNIX
		bool removable;
		struct stat sockinfo;
		struct stat fileinfo;

		removable = sockpath != NULL &&
#if _FFR_MILTER_ROOT_UNSAFE
			    geteuid() != 0 &&
#endif /* _FFR_MILTER_ROOT_UNSAFE */
			    fstat(listenfd, &sockinfo) == 0 &&
			    (S_ISFIFO(sockinfo.st_mode)
# ifdef S_ISSOCK
			     || S_ISSOCK(sockinfo.st_mode)
# endif /* S_ISSOCK */
			    );
#endif /* NETUNIX */

		(void) closesocket(listenfd);
		listenfd = INVALID_SOCKET;

#if NETUNIX
		/* XXX sleep() some time before doing this? */
		if (sockpath != NULL)
		{
			if (removable &&
			    stat(sockpath, &fileinfo) == 0 &&
			    ((fileinfo.st_dev == sockinfo.st_dev &&
			      fileinfo.st_ino == sockinfo.st_ino)
# ifdef S_ISSOCK
			     || S_ISSOCK(fileinfo.st_mode)
# endif /* S_ISSOCK */
			    )
			    &&
			    (S_ISFIFO(fileinfo.st_mode)
# ifdef S_ISSOCK
			     || S_ISSOCK(fileinfo.st_mode)
# endif /* S_ISSOCK */
			     ))
				(void) unlink(sockpath);
			free(sockpath);
			sockpath = NULL;
		}
#endif /* NETUNIX */
	}
	(void) smutex_unlock(&L_Mutex);
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

#if BROKEN_PTHREAD_SLEEP

/*
**  Solaris 2.6, perhaps others, gets an internal threads library panic
**  when sleep() is used:
**
**  thread_create() failed, returned 11 (EINVAL)
**  co_enable, thr_create() returned error = 24
**  libthread panic: co_enable failed (PID: 17793 LWP 1)
**  stacktrace:
**	ef526b10
**	ef52646c
**	ef534cbc
**	156a4
**	14644
**	1413c
**	135e0
**	0
*/

# define MI_SLEEP(s)							\
{									\
	int rs = 0;							\
	struct timeval st;						\
									\
	st.tv_sec = (s);						\
	st.tv_usec = 0;							\
	if (st.tv_sec > 0)						\
		rs = select(0, NULL, NULL, NULL, &st);			\
	if (rs != 0)							\
	{								\
		smi_log(SMI_LOG_ERR,					\
			"MI_SLEEP(): select() returned non-zero result %d, errno = %d",								\
			rs, errno);					\
	}								\
}
#else /* BROKEN_PTHREAD_SLEEP */
# define MI_SLEEP(s)	sleep((s))
#endif /* BROKEN_PTHREAD_SLEEP */

int
mi_listener(conn, dbg, smfi, timeout, backlog)
	char *conn;
	int dbg;
	smfiDesc_ptr smfi;
	time_t timeout;
	int backlog;
{
	socket_t connfd = INVALID_SOCKET;
	int family = AF_UNSPEC;
	int sockopt = 1;
	int r;
	int ret = MI_SUCCESS;
	int mcnt = 0;	/* error count for malloc() failures */
	int tcnt = 0;	/* error count for thread_create() failures */
	int acnt = 0;	/* error count for accept() failures */
	int scnt = 0;	/* error count for select() failures */
	int save_errno = 0;
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
	(void) smutex_init(&L_Mutex);
	(void) smutex_lock(&L_Mutex);
	listenfd = mi_milteropen(conn, backlog, &socksize, &family,
				 smfi->xxfi_name);
	if (!ValidSocket(listenfd))
	{
		smi_log(SMI_LOG_FATAL,
			"%s: Unable to create listening socket on conn %s",
			smfi->xxfi_name, conn);
		(void) smutex_unlock(&L_Mutex);
		return MI_FAILURE;
	}
	clilen = socksize;

	if (listenfd >= FD_SETSIZE)
	{
		smi_log(SMI_LOG_ERR, "%s: fd %d is larger than FD_SETSIZE %d",
			smfi->xxfi_name, listenfd, FD_SETSIZE);
		(void) smutex_unlock(&L_Mutex);
		return MI_FAILURE;
	}
	(void) smutex_unlock(&L_Mutex);

	while (mi_stop() == MILTER_CONT)
	{
		(void) smutex_lock(&L_Mutex);
		if (!ValidSocket(listenfd))
		{
			(void) smutex_unlock(&L_Mutex);
			break;
		}

		/* select on interface ports */
		FD_ZERO(&readset);
		FD_ZERO(&excset);
		FD_SET((unsigned int) listenfd, &readset);
		FD_SET((unsigned int) listenfd, &excset);
		chktime.tv_sec = MI_CHK_TIME;
		chktime.tv_usec = 0;
		r = select(listenfd + 1, &readset, NULL, &excset, &chktime);
		if (r == 0)		/* timeout */
		{
			(void) smutex_unlock(&L_Mutex);
			continue;	/* just check mi_stop() */
		}
		if (r < 0)
		{
			save_errno = errno;
			(void) smutex_unlock(&L_Mutex);
			if (save_errno == EINTR)
				continue;
			scnt++;
			smi_log(SMI_LOG_ERR,
				"%s: select() failed (%s), %s",
				smfi->xxfi_name, sm_errstring(save_errno),
				scnt >= MAX_FAILS_S ? "abort" : "try again");
			MI_SLEEP(scnt);
			if (scnt >= MAX_FAILS_S)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		if (!FD_ISSET(listenfd, &readset))
		{
			/* some error: just stop for now... */
			ret = MI_FAILURE;
			(void) smutex_unlock(&L_Mutex);
			smi_log(SMI_LOG_ERR,
				"%s: select() returned exception for socket, abort",
				smfi->xxfi_name);
			break;
		}
		scnt = 0;	/* reset error counter for select() */

		memset(&cliaddr, '\0', sizeof cliaddr);
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr,
				&clilen);
		save_errno = errno;
		(void) smutex_unlock(&L_Mutex);

		/*
		**  If remote side closes before
		**  accept() finishes, sockaddr
		**  might not be fully filled in.
		*/

		if (ValidSocket(connfd) &&
		    (clilen == 0 ||
# ifdef BSD4_4_SOCKADDR
		     cliaddr.sa.sa_len == 0 ||
# endif /* BSD4_4_SOCKADDR */
		     cliaddr.sa.sa_family != family))
		{
			(void) closesocket(connfd);
			connfd = INVALID_SOCKET;
			save_errno = EINVAL;
		}

		if (!ValidSocket(connfd))
		{
			if (save_errno == EINTR)
				continue;
			acnt++;
			smi_log(SMI_LOG_ERR,
				"%s: accept() returned invalid socket (%s), %s",
				smfi->xxfi_name, sm_errstring(save_errno),
				acnt >= MAX_FAILS_A ? "abort" : "try again");
			MI_SLEEP(acnt);
			if (acnt >= MAX_FAILS_A)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		acnt = 0;	/* reset error counter for accept() */

		if (setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE,
				(void *) &sockopt, sizeof sockopt) < 0)
		{
			smi_log(SMI_LOG_WARN, "%s: setsockopt() failed (%s)",
				smfi->xxfi_name, sm_errstring(errno));
			/* XXX: continue? */
		}
		if ((ctx = (SMFICTX_PTR) malloc(sizeof *ctx)) == NULL)
		{
			(void) closesocket(connfd);
			mcnt++;
			smi_log(SMI_LOG_ERR, "%s: malloc(ctx) failed (%s), %s",
				smfi->xxfi_name, sm_errstring(save_errno),
				mcnt >= MAX_FAILS_M ? "abort" : "try again");
			MI_SLEEP(mcnt);
			if (mcnt >= MAX_FAILS_M)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		mcnt = 0;	/* reset error counter for malloc() */
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
					(void *) ctx)) != 0)
		{
			tcnt++;
			smi_log(SMI_LOG_ERR,
				"%s: thread_create() failed: %d, %s",
				smfi->xxfi_name,  r,
				tcnt >= MAX_FAILS_T ? "abort" : "try again");
			MI_SLEEP(tcnt);
			(void) closesocket(connfd);
			free(ctx);
			if (tcnt >= MAX_FAILS_T)
			{
				ret = MI_FAILURE;
				break;
			}
			continue;
		}
		tcnt = 0;
	}
	if (ret != MI_SUCCESS)
		mi_stop_milters(MILTER_ABRT);
	else
		mi_closener();
	(void) smutex_destroy(&L_Mutex);
	return ret;
}
