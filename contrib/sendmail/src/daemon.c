/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <errno.h>
#include "sendmail.h"

#ifndef lint
#ifdef DAEMON
static char sccsid[] = "@(#)daemon.c	8.220 (Berkeley) 6/24/98 (with daemon mode)";
#else
static char sccsid[] = "@(#)daemon.c	8.220 (Berkeley) 6/24/98 (without daemon mode)";
#endif
#endif /* not lint */

#if defined(SOCK_STREAM) || defined(__GNU_LIBRARY__)
# define USE_SOCK_STREAM	1
#endif

#if DAEMON || defined(USE_SOCK_STREAM)
# include <arpa/inet.h>
# if NAMED_BIND
#  include <resolv.h>
#  ifndef NO_DATA
#   define NO_DATA	NO_ADDRESS
#  endif
# endif
#endif

#if DAEMON

# include <sys/time.h>

# if IP_SRCROUTE
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
#  include <netinet/ip_var.h>
# endif

/*
**  DAEMON.C -- routines to use when running as a daemon.
**
**	This entire file is highly dependent on the 4.2 BSD
**	interprocess communication primitives.  No attempt has
**	been made to make this file portable to Version 7,
**	Version 6, MPX files, etc.  If you should try such a
**	thing yourself, I recommend chucking the entire file
**	and starting from scratch.  Basic semantics are:
**
**	getrequests(e)
**		Opens a port and initiates a connection.
**		Returns in a child.  Must set InChannel and
**		OutChannel appropriately.
**	clrdaemon()
**		Close any open files associated with getting
**		the connection; this is used when running the queue,
**		etc., to avoid having extra file descriptors during
**		the queue run and to avoid confusing the network
**		code (if it cares).
**	makeconnection(host, port, outfile, infile, e)
**		Make a connection to the named host on the given
**		port.  Set *outfile and *infile to the files
**		appropriate for communication.  Returns zero on
**		success, else an exit status describing the
**		error.
**	host_map_lookup(map, hbuf, avp, pstat)
**		Convert the entry in hbuf into a canonical form.
*/
/*
**  GETREQUESTS -- open mail IPC port and get requests.
**
**	Parameters:
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Waits until some interesting activity occurs.  When
**		it does, a child is created to process it, and the
**		parent waits for completion.  Return from this
**		routine is always in the child.  The file pointers
**		"InChannel" and "OutChannel" should be set to point
**		to the communication channel.
*/

int		DaemonSocket	= -1;		/* fd describing socket */
SOCKADDR	DaemonAddr;			/* socket for incoming */
int		ListenQueueSize = 10;		/* size of listen queue */
int		TcpRcvBufferSize = 0;		/* size of TCP receive buffer */
int		TcpSndBufferSize = 0;		/* size of TCP send buffer */

void
getrequests(e)
	ENVELOPE *e;
{
	int t;
	bool refusingconnections = TRUE;
	FILE *pidf;
	int socksize;
	u_short port;
#if XDEBUG
	bool j_has_dot;
#endif
	extern void reapchild __P((int));
	extern int opendaemonsocket __P((bool));

	/*
	**  Set up the address for the mailer.
	*/

	switch (DaemonAddr.sa.sa_family)
	{
	  case AF_UNSPEC:
		DaemonAddr.sa.sa_family = AF_INET;
		/* fall through ... */

	  case AF_INET:
		if (DaemonAddr.sin.sin_addr.s_addr == 0)
			DaemonAddr.sin.sin_addr.s_addr = INADDR_ANY;
		port = DaemonAddr.sin.sin_port;
		break;

	  default:
		/* unknown protocol */
		port = 0;
		break;
	}
	if (port == 0)
	{
		register struct servent *sp;

		sp = getservbyname("smtp", "tcp");
		if (sp == NULL)
		{
			syserr("554 service \"smtp\" unknown");
			port = htons(25);
		}
		else
			port = sp->s_port;
	}

	switch (DaemonAddr.sa.sa_family)
	{
	  case AF_INET:
		DaemonAddr.sin.sin_port = port;
		break;

	  default:
		/* unknown protocol */
		break;
	}

	/*
	**  Try to actually open the connection.
	*/

	if (tTd(15, 1))
		printf("getrequests: port 0x%x\n", port);

	/* get a socket for the SMTP connection */
	socksize = opendaemonsocket(TRUE);

	(void) setsignal(SIGCHLD, reapchild);

	/* write the pid to the log file for posterity */
	pidf = safefopen(PidFile, O_WRONLY|O_TRUNC, 0644,
			 SFF_NOLINK|SFF_ROOTOK|SFF_REGONLY|SFF_CREAT);
	if (pidf == NULL)
	{
		sm_syslog(LOG_ERR, NOQID, "unable to write %s", PidFile);
	}
	else
	{
		extern char *CommandLineArgs;

		/* write the process id on line 1 */
		fprintf(pidf, "%ld\n", (long) getpid());

		/* line 2 contains all command line flags */
		fprintf(pidf, "%s\n", CommandLineArgs);

		/* flush and close */
		fclose(pidf);
	}

#if XDEBUG
	{
		char jbuf[MAXHOSTNAMELEN];

		expand("\201j", jbuf, sizeof jbuf, e);
		j_has_dot = strchr(jbuf, '.') != NULL;
	}
#endif

	if (tTd(15, 1))
		printf("getrequests: %d\n", DaemonSocket);

	for (;;)
	{
		register pid_t pid;
		auto SOCKADDR_LEN_T lotherend;
		int savederrno;
		int pipefd[2];
		extern bool refuseconnections __P((int));

		/* see if we are rejecting connections */
		(void) blocksignal(SIGALRM);
		if (refuseconnections(ntohs(port)))
		{
			if (DaemonSocket >= 0)
			{
				/* close socket so peer will fail quickly */
				(void) close(DaemonSocket);
				DaemonSocket = -1;
			}
			refusingconnections = TRUE;
			sleep(15);
			continue;
		}

		/* arrange to (re)open the socket if necessary */
		if (refusingconnections)
		{
			(void) opendaemonsocket(FALSE);
			refusingconnections = FALSE;
		}

#if XDEBUG
		/* check for disaster */
		{
			char jbuf[MAXHOSTNAMELEN];
			extern void dumpstate __P((char *));

			expand("\201j", jbuf, sizeof jbuf, e);
			if (!wordinclass(jbuf, 'w'))
			{
				dumpstate("daemon lost $j");
				sm_syslog(LOG_ALERT, NOQID,
					"daemon process doesn't have $j in $=w; see syslog");
				abort();
			}
			else if (j_has_dot && strchr(jbuf, '.') == NULL)
			{
				dumpstate("daemon $j lost dot");
				sm_syslog(LOG_ALERT, NOQID,
					"daemon process $j lost dot; see syslog");
				abort();
			}
		}
#endif

		/* wait for a connection */
		setproctitle("accepting connections on port %d",
			     ntohs(port));
#if 0
		/*
		**  Andrew Sun <asun@ieps-sun.ml.com> claims that this will
		**  fix the SVr4 problem.  But it seems to have gone away,
		**  so is it worth doing this?
		*/

		if (SetNonBlocking(DaemonSocket, FALSE) < 0)
			log an error here;
#endif
		(void) releasesignal(SIGALRM);
		for (;;)
		{
			fd_set readfds;
			struct timeval timeout;

			FD_ZERO(&readfds);
			FD_SET(DaemonSocket, &readfds);
			timeout.tv_sec = 60;
			timeout.tv_usec = 0;

			t = select(DaemonSocket + 1, FDSET_CAST &readfds,
				   NULL, NULL, &timeout);
			if (DoQueueRun)
				(void) runqueue(TRUE, FALSE);
			if (t <= 0 || !FD_ISSET(DaemonSocket, &readfds))
				continue;

			errno = 0;
			lotherend = socksize;
			t = accept(DaemonSocket,
			    (struct sockaddr *)&RealHostAddr, &lotherend);
			if (t >= 0 || errno != EINTR)
				break;
		}
		savederrno = errno;
		(void) blocksignal(SIGALRM);
		if (t < 0)
		{
			errno = savederrno;
			syserr("getrequests: accept");

			/* arrange to re-open the socket next time around */
			(void) close(DaemonSocket);
			DaemonSocket = -1;
			refusingconnections = TRUE;
			sleep(5);
			continue;
		}

		/*
		**  Create a subprocess to process the mail.
		*/

		if (tTd(15, 2))
			printf("getrequests: forking (fd = %d)\n", t);

		/*
		**  Create a pipe to keep the child from writing to the
		**  socket until after the parent has closed it.  Otherwise
		**  the parent may hang if the child has closed it first.
		*/

		if (pipe(pipefd) < 0)
			pipefd[0] = pipefd[1] = -1;

		blocksignal(SIGCHLD);
		pid = fork();
		if (pid < 0)
		{
			syserr("daemon: cannot fork");
			if (pipefd[0] != -1)
			{
				(void) close(pipefd[0]);
				(void) close(pipefd[1]);
			}
			(void) releasesignal(SIGCHLD);
			sleep(10);
			(void) close(t);
			continue;
		}

		if (pid == 0)
		{
			char *p;
			extern SIGFUNC_DECL intsig __P((int));
			FILE *inchannel, *outchannel;

			/*
			**  CHILD -- return to caller.
			**	Collect verified idea of sending host.
			**	Verify calling user id if possible here.
			*/

			(void) releasesignal(SIGALRM);
			(void) releasesignal(SIGCHLD);
			(void) setsignal(SIGCHLD, SIG_DFL);
			(void) setsignal(SIGHUP, intsig);
			(void) close(DaemonSocket);
			proc_list_clear();

			/* don't schedule queue runs if we are told to ETRN */
			QueueIntvl = 0;

			setproctitle("startup with %s",
				anynet_ntoa(&RealHostAddr));

			if (pipefd[0] != -1)
			{
				auto char c;

				/*
				**  Wait for the parent to close the write end
				**  of the pipe, which we will see as an EOF.
				**  This guarantees that we won't write to the
				**  socket until after the parent has closed
				**  the pipe.
				*/

				/* close the write end of the pipe */
				(void) close(pipefd[1]);

				/* we shouldn't be interrupted, but ... */
				while (read(pipefd[0], &c, 1) < 0 &&
				       errno == EINTR)
					continue;
				(void) close(pipefd[0]);
			}

			/* determine host name */
			p = hostnamebyanyaddr(&RealHostAddr);
			if (strlen(p) > (SIZE_T) MAXNAME)
				p[MAXNAME] = '\0';
			RealHostName = newstr(p);
			setproctitle("startup with %s", p);

			if ((inchannel = fdopen(t, "r")) == NULL ||
			    (t = dup(t)) < 0 ||
			    (outchannel = fdopen(t, "w")) == NULL)
			{
				syserr("cannot open SMTP server channel, fd=%d", t);
				exit(EX_OK);
			}

			InChannel = inchannel;
			OutChannel = outchannel;
			DisConnected = FALSE;

			/* open maps for check_relay ruleset */
			initmaps(FALSE, e);

#ifdef XLA
			if (!xla_host_ok(RealHostName))
			{
				message("421 Too many SMTP sessions for this host");
				exit(EX_OK);
			}
#endif

			break;
		}

		/* parent -- keep track of children */
		proc_list_add(pid);
		(void) releasesignal(SIGCHLD);

		/* close the read end of the synchronization pipe */
		if (pipefd[0] != -1)
			(void) close(pipefd[0]);

		/* close the port so that others will hang (for a while) */
		(void) close(t);

		/* release the child by closing the read end of the sync pipe */
		if (pipefd[1] != -1)
			(void) close(pipefd[1]);
	}
	if (tTd(15, 2))
		printf("getreq: returning\n");
	return;
}
/*
**  OPENDAEMONSOCKET -- open the SMTP socket
**
**	Deals with setting all appropriate options.  DaemonAddr must
**	be set up in advance.
**
**	Parameters:
**		firsttime -- set if this is the initial open.
**
**	Returns:
**		Size in bytes of the daemon socket addr.
**
**	Side Effects:
**		Leaves DaemonSocket set to the open socket.
**		Exits if the socket cannot be created.
*/

#define MAXOPENTRIES	10	/* maximum number of tries to open connection */

int
opendaemonsocket(firsttime)
	bool firsttime;
{
	int on = 1;
	int socksize = 0;
	int ntries = 0;
	int saveerrno;

	if (tTd(15, 2))
		printf("opendaemonsocket()\n");

	do
	{
		if (ntries > 0)
			sleep(5);
		if (firsttime || DaemonSocket < 0)
		{
			DaemonSocket = socket(DaemonAddr.sa.sa_family, SOCK_STREAM, 0);
			if (DaemonSocket < 0)
			{
				saveerrno = errno;
				syserr("opendaemonsocket: can't create server SMTP socket");
			  severe:
				if (LogLevel > 0)
					sm_syslog(LOG_ALERT, NOQID,
						"problem creating SMTP socket");
				DaemonSocket = -1;
				continue;
			}

			/* turn on network debugging? */
			if (tTd(15, 101))
				(void) setsockopt(DaemonSocket, SOL_SOCKET,
						  SO_DEBUG, (char *)&on,
						  sizeof on);

			(void) setsockopt(DaemonSocket, SOL_SOCKET,
					  SO_REUSEADDR, (char *)&on, sizeof on);
			(void) setsockopt(DaemonSocket, SOL_SOCKET,
					  SO_KEEPALIVE, (char *)&on, sizeof on);

#ifdef SO_RCVBUF
			if (TcpRcvBufferSize > 0)
			{
				if (setsockopt(DaemonSocket, SOL_SOCKET,
					       SO_RCVBUF,
					       (char *) &TcpRcvBufferSize,
					       sizeof(TcpRcvBufferSize)) < 0)
					syserr("opendaemonsocket: setsockopt(SO_RCVBUF)");
			}
#endif

			switch (DaemonAddr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				socksize = sizeof DaemonAddr.sin;
				break;
# endif

# if NETISO
			  case AF_ISO:
				socksize = sizeof DaemonAddr.siso;
				break;
# endif

			  default:
				socksize = sizeof DaemonAddr;
				break;
			}

			if (bind(DaemonSocket, &DaemonAddr.sa, socksize) < 0)
			{
				/* probably another daemon already */
				saveerrno = errno;
				syserr("opendaemonsocket: cannot bind");
				(void) close(DaemonSocket);
				goto severe;
			}
		}
		if (!firsttime && listen(DaemonSocket, ListenQueueSize) < 0)
		{
			saveerrno = errno;
			syserr("opendaemonsocket: cannot listen");
			(void) close(DaemonSocket);
			goto severe;
		}
		return socksize;
	} while (ntries++ < MAXOPENTRIES && transienterror(saveerrno));
	syserr("!opendaemonsocket: server SMTP socket wedged: exiting");
	finis();
	return -1;  /* avoid compiler warning on IRIX */
}
/*
**  CLRDAEMON -- reset the daemon connection
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		releases any resources used by the passive daemon.
*/

void
clrdaemon()
{
	if (DaemonSocket >= 0)
		(void) close(DaemonSocket);
	DaemonSocket = -1;
}
/*
**  SETDAEMONOPTIONS -- set options for running the daemon
**
**	Parameters:
**		p -- the options line.
**
**	Returns:
**		none.
*/

void
setdaemonoptions(p)
	register char *p;
{
	if (DaemonAddr.sa.sa_family == AF_UNSPEC)
		DaemonAddr.sa.sa_family = AF_INET;

	while (p != NULL)
	{
		register char *f;
		register char *v;

		while (isascii(*p) && isspace(*p))
			p++;
		if (*p == '\0')
			break;
		f = p;
		p = strchr(p, ',');
		if (p != NULL)
			*p++ = '\0';
		v = strchr(f, '=');
		if (v == NULL)
			continue;
		while (isascii(*++v) && isspace(*v))
			continue;
		if (isascii(*f) && islower(*f))
			*f = toupper(*f);

		switch (*f)
		{
		  case 'F':		/* address family */
			if (isascii(*v) && isdigit(*v))
				DaemonAddr.sa.sa_family = atoi(v);
#if NETINET
			else if (strcasecmp(v, "inet") == 0)
				DaemonAddr.sa.sa_family = AF_INET;
#endif
#if NETISO
			else if (strcasecmp(v, "iso") == 0)
				DaemonAddr.sa.sa_family = AF_ISO;
#endif
#if NETNS
			else if (strcasecmp(v, "ns") == 0)
				DaemonAddr.sa.sa_family = AF_NS;
#endif
#if NETX25
			else if (strcasecmp(v, "x.25") == 0)
				DaemonAddr.sa.sa_family = AF_CCITT;
#endif
			else
				syserr("554 Unknown address family %s in Family=option", v);
			break;

		  case 'A':		/* address */
			switch (DaemonAddr.sa.sa_family)
			{
#if NETINET
			  case AF_INET:
				if (isascii(*v) && isdigit(*v))
					DaemonAddr.sin.sin_addr.s_addr = inet_addr(v);
				else
				{
					register struct hostent *hp;

					hp = sm_gethostbyname(v);
					if (hp == NULL)
						syserr("554 host \"%s\" unknown", v);
					else
						bcopy(hp->h_addr, &DaemonAddr.sin.sin_addr, INADDRSZ);
				}
				break;
#endif

			  default:
				syserr("554 Address= option unsupported for family %d",
					DaemonAddr.sa.sa_family);
				break;
			}
			break;

		  case 'P':		/* port */
			switch (DaemonAddr.sa.sa_family)
			{
#if NETISO
				short port;
#endif

#if NETINET
			  case AF_INET:
				if (isascii(*v) && isdigit(*v))
					DaemonAddr.sin.sin_port = htons(atoi(v));
				else
				{
					register struct servent *sp;

					sp = getservbyname(v, "tcp");
					if (sp == NULL)
						syserr("554 service \"%s\" unknown", v);
					else
						DaemonAddr.sin.sin_port = sp->s_port;
				}
				break;
#endif

#if NETISO
			  case AF_ISO:
				/* assume two byte transport selector */
				if (isascii(*v) && isdigit(*v))
					port = htons(atoi(v));
				else
				{
					register struct servent *sp;

					sp = getservbyname(v, "tcp");
					if (sp == NULL)
						syserr("554 service \"%s\" unknown", v);
					else
						port = sp->s_port;
				}
				bcopy((char *) &port, TSEL(&DaemonAddr.siso), 2);
				break;
#endif

			  default:
				syserr("554 Port= option unsupported for family %d",
					DaemonAddr.sa.sa_family);
				break;
			}
			break;

		  case 'L':		/* listen queue size */
			ListenQueueSize = atoi(v);
			break;

		  case 'S':		/* send buffer size */
			TcpSndBufferSize = atoi(v);
			break;

		  case 'R':		/* receive buffer size */
			TcpRcvBufferSize = atoi(v);
			break;

		  default:
			syserr("554 DaemonPortOptions parameter \"%s\" unknown", f);
		}
	}
}
/*
**  MAKECONNECTION -- make a connection to an SMTP socket on another machine.
**
**	Parameters:
**		host -- the name of the host.
**		port -- the port number to connect to.
**		mci -- a pointer to the mail connection information
**			structure to be filled in.
**		e -- the current envelope.
**
**	Returns:
**		An exit code telling whether the connection could be
**			made and if not why not.
**
**	Side Effects:
**		none.
*/

static jmp_buf	CtxConnectTimeout;

static void
connecttimeout()
{
	errno = ETIMEDOUT;
	longjmp(CtxConnectTimeout, 1);
}

SOCKADDR	CurHostAddr;		/* address of current host */

int
makeconnection(host, port, mci, e)
	char *host;
	u_short port;
	register MCI *mci;
	ENVELOPE *e;
{
	register volatile int addrno = 0;
	register volatile int s;
	register struct hostent *volatile hp = (struct hostent *)NULL;
	SOCKADDR addr;
	int sav_errno;
	volatile int addrlen;
	volatile bool firstconnect;
	EVENT *volatile ev = NULL;

	/*
	**  Set up the address for the mailer.
	**	Accept "[a.b.c.d]" syntax for host name.
	*/

#if NAMED_BIND
	h_errno = 0;
#endif
	errno = 0;
	bzero(&CurHostAddr, sizeof CurHostAddr);
	SmtpPhase = mci->mci_phase = "initial connection";
	CurHostName = host;

	if (host[0] == '[')
	{
#if NETINET
		unsigned long hid = INADDR_NONE;
#endif
		register char *p = strchr(host, ']');

		if (p != NULL)
		{
			*p = '\0';
#if NETINET
			hid = inet_addr(&host[1]);
			if (hid == INADDR_NONE)
#endif
			{
				/* try it as a host name (avoid MX lookup) */
				hp = sm_gethostbyname(&host[1]);
				if (hp == NULL && p[-1] == '.')
				{
#if NAMED_BIND
					int oldopts = _res.options;

					_res.options &= ~(RES_DEFNAMES|RES_DNSRCH);
#endif
					p[-1] = '\0';
					hp = sm_gethostbyname(&host[1]);
					p[-1] = '.';
#if NAMED_BIND
					_res.options = oldopts;
#endif
				}
				*p = ']';
				goto gothostent;
			}
			*p = ']';
		}
		if (p == NULL)
		{
			extern char MsgBuf[];

			usrerr("553 Invalid numeric domain spec \"%s\"", host);
			mci_setstat(mci, EX_NOHOST, "5.1.2", MsgBuf);
			return EX_NOHOST;
		}
#if NETINET
		addr.sin.sin_family = AF_INET;		/*XXX*/
		addr.sin.sin_addr.s_addr = hid;
#endif
	}
	else
	{
		/* contortion to get around SGI cc complaints */
		{
			register char *p = &host[strlen(host) - 1];

			hp = sm_gethostbyname(host);
			if (hp == NULL && *p == '.')
			{
#if NAMED_BIND
				int oldopts = _res.options;

				_res.options &= ~(RES_DEFNAMES|RES_DNSRCH);
#endif
				*p = '\0';
				hp = sm_gethostbyname(host);
				*p = '.';
#if NAMED_BIND
				_res.options = oldopts;
#endif
			}
		}
gothostent:
		if (hp == NULL)
		{
#if NAMED_BIND
			/* check for name server timeouts */
			if (errno == ETIMEDOUT || h_errno == TRY_AGAIN ||
			    (errno == ECONNREFUSED && UseNameServer))
			{
				mci_setstat(mci, EX_TEMPFAIL, "4.4.3", NULL);
				return EX_TEMPFAIL;
			}
#endif
			mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
			return (EX_NOHOST);
		}
		addr.sa.sa_family = hp->h_addrtype;
		switch (hp->h_addrtype)
		{
#if NETINET
		  case AF_INET:
			bcopy(hp->h_addr,
				&addr.sin.sin_addr,
				INADDRSZ);
			break;
#endif

		  default:
			if (hp->h_length > sizeof addr.sa.sa_data)
			{
				syserr("makeconnection: long sa_data: family %d len %d",
					hp->h_addrtype, hp->h_length);
				mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
				return EX_NOHOST;
			}
			bcopy(hp->h_addr,
				addr.sa.sa_data,
				hp->h_length);
			break;
		}
		addrno = 1;
	}

	/*
	**  Determine the port number.
	*/

	if (port == 0)
	{
		register struct servent *sp = getservbyname("smtp", "tcp");

		if (sp == NULL)
		{
			if (LogLevel > 2)
				sm_syslog(LOG_ERR, NOQID,
					"makeconnection: service \"smtp\" unknown");
			port = htons(25);
		}
		else
			port = sp->s_port;
	}

	switch (addr.sa.sa_family)
	{
#if NETINET
	  case AF_INET:
		addr.sin.sin_port = port;
		addrlen = sizeof (struct sockaddr_in);
		break;
#endif

#if NETISO
	  case AF_ISO:
		/* assume two byte transport selector */
		bcopy((char *) &port, TSEL((struct sockaddr_iso *) &addr), 2);
		addrlen = sizeof (struct sockaddr_iso);
		break;
#endif

	  default:
		syserr("Can't connect to address family %d", addr.sa.sa_family);
		mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
		return (EX_NOHOST);
	}

	/*
	**  Try to actually open the connection.
	*/

#ifdef XLA
	/* if too many connections, don't bother trying */
	if (!xla_noqueue_ok(host))
		return EX_TEMPFAIL;
#endif

	firstconnect = TRUE;
	for (;;)
	{
		if (tTd(16, 1))
			printf("makeconnection (%s [%s])\n",
				host, anynet_ntoa(&addr));

		/* save for logging */
		CurHostAddr = addr;

		if (bitnset(M_SECURE_PORT, mci->mci_mailer->m_flags))
		{
			int rport = IPPORT_RESERVED - 1;

			s = rresvport(&rport);
		}
		else
		{
			s = socket(addr.sa.sa_family, SOCK_STREAM, 0);
		}
		if (s < 0)
		{
			sav_errno = errno;
			syserr("makeconnection: cannot create socket");
#ifdef XLA
			xla_host_end(host);
#endif
			mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
			return EX_TEMPFAIL;
		}

#ifdef SO_SNDBUF
		if (TcpSndBufferSize > 0)
		{
			if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
				       (char *) &TcpSndBufferSize,
				       sizeof(TcpSndBufferSize)) < 0)
				syserr("makeconnection: setsockopt(SO_SNDBUF)");
		}
#endif

		if (tTd(16, 1))
			printf("makeconnection: fd=%d\n", s);

		/* turn on network debugging? */
		if (tTd(16, 101))
		{
			int on = 1;
			(void) setsockopt(s, SOL_SOCKET, SO_DEBUG,
					  (char *)&on, sizeof on);
		}
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);		/* for debugging */
		errno = 0;					/* for debugging */

		/*
		**  Linux seems to hang in connect for 90 minutes (!!!).
		**  Time out the connect to avoid this problem.
		*/

		if (setjmp(CtxConnectTimeout) == 0)
		{
			int i;

			if (e->e_ntries <= 0 && TimeOuts.to_iconnect != 0)
				ev = setevent(TimeOuts.to_iconnect, connecttimeout, 0);
			else if (TimeOuts.to_connect != 0)
				ev = setevent(TimeOuts.to_connect, connecttimeout, 0);
			else
				ev = NULL;

#if _FFR_CONNECTONLYTO_OPTION
			/* for testing */
			if (ConnectOnlyTo != 0)
				addr.sin.sin_addr.s_addr = ConnectOnlyTo;
#endif
			i = connect(s, (struct sockaddr *) &addr, addrlen);
			sav_errno = errno;
			if (ev != NULL)
				clrevent(ev);
			if (i >= 0)
				break;
		}
		else
			sav_errno = errno;

		/* if running demand-dialed connection, try again */
		if (DialDelay > 0 && firstconnect)
		{
			if (tTd(16, 1))
				printf("Connect failed (%s); trying again...\n",
					errstring(sav_errno));
			firstconnect = FALSE;
			sleep(DialDelay);
			continue;
		}

		/* couldn't connect.... figure out why */
		(void) close(s);

		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, e->e_id,
				  "makeconnection (%s [%s]) failed: %s",
				  host, anynet_ntoa(&addr),
				  errstring(sav_errno));

		if (hp != NULL && hp->h_addr_list[addrno] != NULL)
		{
			if (tTd(16, 1))
				printf("Connect failed (%s); trying new address....\n",
					errstring(sav_errno));
			switch (addr.sa.sa_family)
			{
#if NETINET
			  case AF_INET:
				bcopy(hp->h_addr_list[addrno++],
				      &addr.sin.sin_addr,
				      INADDRSZ);
				break;
#endif

			  default:
				bcopy(hp->h_addr_list[addrno++],
					addr.sa.sa_data,
					hp->h_length);
				break;
			}
			continue;
		}

		/* couldn't open connection */
#ifdef XLA
		xla_host_end(host);
#endif
		mci_setstat(mci, EX_TEMPFAIL, "4.4.1", NULL);
		return EX_TEMPFAIL;
	}

	/* connection ok, put it into canonical form */
	if ((mci->mci_out = fdopen(s, "w")) == NULL ||
	    (s = dup(s)) < 0 ||
	    (mci->mci_in = fdopen(s, "r")) == NULL)
	{
		syserr("cannot open SMTP client channel, fd=%d", s);
		mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
		return EX_TEMPFAIL;
	}

	mci_setstat(mci, EX_OK, NULL, NULL);
	return (EX_OK);
}
/*
**  MYHOSTNAME -- return the name of this host.
**
**	Parameters:
**		hostbuf -- a place to return the name of this host.
**		size -- the size of hostbuf.
**
**	Returns:
**		A list of aliases for this host.
**
**	Side Effects:
**		Adds numeric codes to $=w.
*/

struct hostent *
myhostname(hostbuf, size)
	char hostbuf[];
	int size;
{
	register struct hostent *hp;

	if (gethostname(hostbuf, size) < 0)
	{
		(void) strcpy(hostbuf, "localhost");
	}
	hp = sm_gethostbyname(hostbuf);
	if (hp == NULL)
		return NULL;
	if (strchr(hp->h_name, '.') != NULL || strchr(hostbuf, '.') == NULL)
	{
		(void) strncpy(hostbuf, hp->h_name, size - 1);
		hostbuf[size - 1] = '\0';
	}

	/*
	**  If there is still no dot in the name, try looking for a
	**  dotted alias.
	*/

	if (strchr(hostbuf, '.') == NULL)
	{
		char **ha;

		for (ha = hp->h_aliases; *ha != NULL; ha++)
		{
			if (strchr(*ha, '.') != NULL)
			{
				(void) strncpy(hostbuf, *ha, size - 1);
				hostbuf[size - 1] = '\0';
				break;
			}
		}
	}

	/*
	**  If _still_ no dot, wait for a while and try again -- it is
	**  possible that some service is starting up.  This can result
	**  in excessive delays if the system is badly configured, but
	**  there really isn't a way around that, particularly given that
	**  the config file hasn't been read at this point.
	**  All in all, a bit of a mess.
	*/

	if (strchr(hostbuf, '.') == NULL &&
	    !getcanonname(hostbuf, size, TRUE))
	{
		sm_syslog(LOG_CRIT, NOQID,
			"My unqualified host name (%s) unknown; sleeping for retry",
			hostbuf);
		message("My unqualified host name (%s) unknown; sleeping for retry",
			hostbuf);
		sleep(60);
		if (!getcanonname(hostbuf, size, TRUE))
		{
			sm_syslog(LOG_ALERT, NOQID,
				"unable to qualify my own domain name (%s) -- using short name",
				hostbuf);
			message("WARNING: unable to qualify my own domain name (%s) -- using short name",
				hostbuf);
		}
	}
	return (hp);
}
/*
**  ADDRCMP -- compare two host addresses
**
**	Parameters:
**		hp -- hostent structure for the first address
**		ha -- actual first address
**		sa -- second address
**
**	Returns:
**		0 -- if ha and sa match
**		else -- they don't match
*/

int
addrcmp(hp, ha, sa)
	struct hostent *hp;
	char *ha;
	SOCKADDR *sa;
{
	switch (sa->sa.sa_family)
	{
	  case AF_INET:
		if (hp->h_addrtype == AF_INET)
			return bcmp(ha, (char *) &sa->sin.sin_addr, hp->h_length);
		break;

	}
	return -1;
}
/*
**  GETAUTHINFO -- get the real host name asociated with a file descriptor
**
**	Uses RFC1413 protocol to try to get info from the other end.
**
**	Parameters:
**		fd -- the descriptor
**		may_be_forged -- an outage that is set to TRUE if the
**			forward lookup of RealHostName does not match
**			RealHostAddr; set to FALSE if they do match.
**
**	Returns:
**		The user@host information associated with this descriptor.
*/

static jmp_buf	CtxAuthTimeout;

static void
authtimeout()
{
	longjmp(CtxAuthTimeout, 1);
}

char *
getauthinfo(fd, may_be_forged)
	int fd;
	bool *may_be_forged;
{
	SOCKADDR_LEN_T falen;
	register char *volatile p = NULL;
	SOCKADDR la;
	SOCKADDR_LEN_T lalen;
	register struct servent *sp;
	volatile int s;
	int i = 0;
	EVENT *ev;
	int nleft;
	struct hostent *hp;
	char *ostype = NULL;
	char **ha;
	char ibuf[MAXNAME + 1];
	static char hbuf[MAXNAME * 2 + 11];

	*may_be_forged = FALSE;
	falen = sizeof RealHostAddr;
	if (isatty(fd) || (i = getpeername(fd, &RealHostAddr.sa, &falen)) < 0 ||
	    falen <= 0 || RealHostAddr.sa.sa_family == 0)
	{
		if (i < 0 && errno != ENOTSOCK)
			return NULL;
		(void) snprintf(hbuf, sizeof hbuf, "%s@localhost",
			RealUserName);
		if (tTd(9, 1))
			printf("getauthinfo: %s\n", hbuf);
		return hbuf;
	}

	if (RealHostName == NULL)
	{
		/* translate that to a host name */
		RealHostName = newstr(hostnamebyanyaddr(&RealHostAddr));
		if (strlen(RealHostName) > MAXNAME)
			RealHostName[MAXNAME - 1] = '\0';
	}

	/* cross check RealHostName with forward DNS lookup */
	if (anynet_ntoa(&RealHostAddr)[0] == '[' ||
	    RealHostName[0] == '[')
	{
		/*
		** address is not a socket or have an
		** IP address with no forward lookup
		*/
		*may_be_forged = FALSE;
	}
	else
	{
		/* try to match the reverse against the forward lookup */
		hp = sm_gethostbyname(RealHostName);

		if (hp == NULL)
			*may_be_forged = TRUE;
		else
		{
			for (ha = hp->h_addr_list; *ha != NULL; ha++)
				if (addrcmp(hp, *ha, &RealHostAddr) == 0)
					break;
			*may_be_forged = *ha == NULL;
		}
	}

	if (TimeOuts.to_ident == 0)
		goto noident;

	lalen = sizeof la;
	if (RealHostAddr.sa.sa_family != AF_INET ||
	    getsockname(fd, &la.sa, &lalen) < 0 || lalen <= 0 ||
	    la.sa.sa_family != AF_INET)
	{
		/* no ident info */
		goto noident;
	}

	/* create ident query */
	(void) snprintf(ibuf, sizeof ibuf, "%d,%d\r\n",
		ntohs(RealHostAddr.sin.sin_port), ntohs(la.sin.sin_port));

	/* create local address */
	la.sin.sin_port = 0;

	/* create foreign address */
	sp = getservbyname("auth", "tcp");
	if (sp != NULL)
		RealHostAddr.sin.sin_port = sp->s_port;
	else
		RealHostAddr.sin.sin_port = htons(113);

	s = -1;
	if (setjmp(CtxAuthTimeout) != 0)
	{
		if (s >= 0)
			(void) close(s);
		goto noident;
	}

	/* put a timeout around the whole thing */
	ev = setevent(TimeOuts.to_ident, authtimeout, 0);

	/* connect to foreign IDENT server using same address as SMTP socket */
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		clrevent(ev);
		goto noident;
	}
	if (bind(s, &la.sa, sizeof la.sin) < 0 ||
	    connect(s, &RealHostAddr.sa, sizeof RealHostAddr.sin) < 0)
	{
		goto closeident;
	}

	if (tTd(9, 10))
		printf("getauthinfo: sent %s", ibuf);

	/* send query */
	if (write(s, ibuf, strlen(ibuf)) < 0)
		goto closeident;

	/* get result */
	p = &ibuf[0];
	nleft = sizeof ibuf - 1;
	while ((i = read(s, p, nleft)) > 0)
	{
		p += i;
		nleft -= i;
		*p = '\0';
		if (strchr(ibuf, '\n') != NULL)
			break;
	}
	(void) close(s);
	clrevent(ev);
	if (i < 0 || p == &ibuf[0])
		goto noident;

	if (*--p == '\n' && *--p == '\r')
		p--;
	*++p = '\0';

	if (tTd(9, 3))
		printf("getauthinfo:  got %s\n", ibuf);

	/* parse result */
	p = strchr(ibuf, ':');
	if (p == NULL)
	{
		/* malformed response */
		goto noident;
	}
	while (isascii(*++p) && isspace(*p))
		continue;
	if (strncasecmp(p, "userid", 6) != 0)
	{
		/* presumably an error string */
		goto noident;
	}
	p += 6;
	while (isascii(*p) && isspace(*p))
		p++;
	if (*p++ != ':')
	{
		/* either useridxx or malformed response */
		goto noident;
	}

	/* p now points to the OSTYPE field */
	while (isascii(*p) && isspace(*p))
		p++;
	ostype = p;
	p = strchr(p, ':');
	if (p == NULL)
	{
		/* malformed response */
		goto noident;
	}
	else
	{
		char *charset;

		*p = '\0';
		charset = strchr(ostype, ',');
		if (charset != NULL)
			*charset = '\0';
	}

	/* 1413 says don't do this -- but it's broken otherwise */
	while (isascii(*++p) && isspace(*p))
		continue;

	/* p now points to the authenticated name -- copy carefully */
	if (strncasecmp(ostype, "other", 5) == 0 &&
	    (ostype[5] == ' ' || ostype[5] == '\0'))
	{
		snprintf(hbuf, sizeof hbuf, "IDENT:");
		cleanstrcpy(&hbuf[6], p, MAXNAME);
	}
	else
		cleanstrcpy(hbuf, p, MAXNAME);
	i = strlen(hbuf);
	snprintf(&hbuf[i], sizeof hbuf - i, "@%s",
		 RealHostName == NULL ? "localhost" : RealHostName);
	goto postident;

closeident:
	(void) close(s);
	clrevent(ev);

noident:
	if (RealHostName == NULL)
	{
		if (tTd(9, 1))
			printf("getauthinfo: NULL\n");
		return NULL;
	}
	snprintf(hbuf, sizeof hbuf, "%s", RealHostName);

postident:
#if IP_SRCROUTE
# ifndef GET_IPOPT_DST
#  define GET_IPOPT_DST(dst)	(dst)
# endif
	/*
	**  Extract IP source routing information.
	**
	**	Format of output for a connection from site a through b
	**	through c to d:
	**		loose:      @site-c@site-b:site-a
	**		strict:	   !@site-c@site-b:site-a
	**
	**	o - pointer within ipopt_list structure.
	**	q - pointer within ls/ss rr route data
	**	p - pointer to hbuf
	*/

	if (RealHostAddr.sa.sa_family == AF_INET)
	{
		SOCKOPT_LEN_T ipoptlen;
		int j;
		u_char *q;
		u_char *o;
		int l;
		struct in_addr addr;
		struct ipoption ipopt;

		ipoptlen = sizeof ipopt;
		if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS,
			       (char *) &ipopt, &ipoptlen) < 0)
			goto noipsr;
		if (ipoptlen == 0)
			goto noipsr;
		o = (u_char *) ipopt.ipopt_list;
		while (o != NULL && o < (u_char *) &ipopt + ipoptlen)
		{
			switch (*o)
			{
			  case IPOPT_EOL: 
				o = NULL;
				break;

			  case IPOPT_NOP:
				o++;
				break;

			  case IPOPT_SSRR:
			  case IPOPT_LSRR:
				/*
				**  Source routing.
				**	o[0] is the option type (loose/strict).
				**	o[1] is the length of this option,
				**		including option type and
				**		length.
				**	o[2] is the pointer into the route
				**		data.
				**	o[3] begins the route data.
				*/

				p = &hbuf[strlen(hbuf)];
				l = sizeof hbuf - (hbuf - p) - 6;
				snprintf(p, SPACELEFT(hbuf, p), " [%s@%.*s",
				    *o == IPOPT_SSRR ? "!" : "",
				    l > 240 ? 120 : l / 2,
				    inet_ntoa(GET_IPOPT_DST(ipopt.ipopt_dst)));
				i = strlen(p);
				p += i;
				l -= strlen(p);

				j = o[1] / sizeof(struct in_addr) - 1;

				/* q skips length and router pointer to data */
				q = &o[3];
				for ( ; j >= 0; j--)
				{
					memcpy(&addr, q, sizeof(addr));
					snprintf(p, SPACELEFT(hbuf, p),
						"%c%.*s",
						j != 0 ? '@' : ':',
						l > 240 ? 120 :
						    j == 0 ? l : l / 2,
						inet_ntoa(addr));
					i = strlen(p);
					p += i;
					l -= i + 1;
					q += sizeof(struct in_addr); 
				}
				o += o[1];
				break;

			  default:
				/* Skip over option */
				o += o[1];
				break;
			}
		}
		snprintf(p, SPACELEFT(hbuf, p), "]");
		goto postipsr;
	}

noipsr:
#endif
	if (RealHostName != NULL && RealHostName[0] != '[')
	{
		p = &hbuf[strlen(hbuf)];
		(void) snprintf(p, SPACELEFT(hbuf, p), " [%.100s]",
			anynet_ntoa(&RealHostAddr));
	}
	if (*may_be_forged)
	{
		p = &hbuf[strlen(hbuf)];
		(void) snprintf(p, SPACELEFT(hbuf, p), " (may be forged)");
	}

#if IP_SRCROUTE
postipsr:
#endif
	if (tTd(9, 1))
		printf("getauthinfo: %s\n", hbuf);
	return hbuf;
}
/*
**  HOST_MAP_LOOKUP -- turn a hostname into canonical form
**
**	Parameters:
**		map -- a pointer to this map.
**		name -- the (presumably unqualified) hostname.
**		av -- unused -- for compatibility with other mapping
**			functions.
**		statp -- an exit status (out parameter) -- set to
**			EX_TEMPFAIL if the name server is unavailable.
**
**	Returns:
**		The mapping, if found.
**		NULL if no mapping found.
**
**	Side Effects:
**		Looks up the host specified in hbuf.  If it is not
**		the canonical name for that host, return the canonical
**		name (unless MF_MATCHONLY is set, which will cause the
**		status only to be returned).
*/

char *
host_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	register struct hostent *hp;
	struct in_addr in_addr;
	char *cp;
	register STAB *s;
	char hbuf[MAXNAME + 1];

	/*
	**  See if we have already looked up this name.  If so, just
	**  return it.
	*/

	s = stab(name, ST_NAMECANON, ST_ENTER);
	if (bitset(NCF_VALID, s->s_namecanon.nc_flags))
	{
		if (tTd(9, 1))
			printf("host_map_lookup(%s) => CACHE %s\n",
			       name,
			       s->s_namecanon.nc_cname == NULL
					? "NULL"
					: s->s_namecanon.nc_cname);
		errno = s->s_namecanon.nc_errno;
#if NAMED_BIND
		h_errno = s->s_namecanon.nc_herrno;
#endif
		*statp = s->s_namecanon.nc_stat;
		if (*statp == EX_TEMPFAIL)
		{
			CurEnv->e_status = "4.4.3";
			message("851 %s: Name server timeout",
				shortenstring(name, 33));
		}
		if (*statp != EX_OK)
			return NULL;
		if (s->s_namecanon.nc_cname == NULL)
		{
			syserr("host_map_lookup(%s): bogus NULL cache entry, errno = %d, h_errno = %d",
				name,
				s->s_namecanon.nc_errno,
				s->s_namecanon.nc_herrno);
			return NULL;
		}
		if (bitset(MF_MATCHONLY, map->map_mflags))
			cp = map_rewrite(map, name, strlen(name), NULL);
		else
			cp = map_rewrite(map,
					 s->s_namecanon.nc_cname,
					 strlen(s->s_namecanon.nc_cname),
					 av);
		return cp;
	}

	/*
	**  If we are running without a regular network connection (usually
	**  dial-on-demand) and we are just queueing, we want to avoid DNS
	**  lookups because those could try to connect to a server.
	*/

	if (CurEnv->e_sendmode == SM_DEFER)
	{
		if (tTd(9, 1))
			printf("host_map_lookup(%s) => DEFERRED\n", name);
		*statp = EX_TEMPFAIL;
		return NULL;
	}

	/*
	**  If first character is a bracket, then it is an address
	**  lookup.  Address is copied into a temporary buffer to
	**  strip the brackets and to preserve name if address is
	**  unknown.
	*/

	if (*name != '[')
	{
		if (tTd(9, 1))
			printf("host_map_lookup(%s) => ", name);
		s->s_namecanon.nc_flags |= NCF_VALID;		/* will be soon */
		snprintf(hbuf, sizeof hbuf, "%s", name);
		if (getcanonname(hbuf, sizeof hbuf - 1, !HasWildcardMX))
		{
			if (tTd(9, 1))
				printf("%s\n", hbuf);
			s->s_namecanon.nc_stat = EX_OK;
			s->s_namecanon.nc_cname = newstr(hbuf);
			if (bitset(MF_MATCHONLY, map->map_mflags))
				cp = map_rewrite(map, name, strlen(name), NULL);
			else
				cp = map_rewrite(map, hbuf, strlen(hbuf), av);
			return cp;
		}
		else
		{
			s->s_namecanon.nc_errno = errno;
#if NAMED_BIND
			s->s_namecanon.nc_herrno = h_errno;
			if (tTd(9, 1))
				printf("FAIL (%d)\n", h_errno);
			switch (h_errno)
			{
			  case TRY_AGAIN:
				if (UseNameServer)
				{
					CurEnv->e_status = "4.4.3";
					message("851 %s: Name server timeout",
						shortenstring(name, 33));
				}
				*statp = EX_TEMPFAIL;
				break;

			  case HOST_NOT_FOUND:
			  case NO_DATA:
				*statp = EX_NOHOST;
				break;

			  case NO_RECOVERY:
				*statp = EX_SOFTWARE;
				break;

			  default:
				*statp = EX_UNAVAILABLE;
				break;
			}
#else
			if (tTd(9, 1))
				printf("FAIL\n");
			*statp = EX_NOHOST;
#endif
			s->s_namecanon.nc_stat = *statp;
			return NULL;
		}
	}
	if ((cp = strchr(name, ']')) == NULL)
		return (NULL);
	*cp = '\0';
	in_addr.s_addr = inet_addr(&name[1]);
	*cp = ']';

	/* nope -- ask the name server */
	hp = sm_gethostbyaddr((char *)&in_addr, INADDRSZ, AF_INET);
	s->s_namecanon.nc_errno = errno;
#if NAMED_BIND
	s->s_namecanon.nc_herrno = h_errno;
#endif
	s->s_namecanon.nc_flags |= NCF_VALID;		/* will be soon */
	if (hp == NULL)
	{
		s->s_namecanon.nc_stat = *statp = EX_NOHOST;
		return (NULL);
	}

	/* found a match -- copy out */
	hp->h_name = denlstring((char *) hp->h_name, TRUE, TRUE);
	s->s_namecanon.nc_stat = *statp = EX_OK;
	s->s_namecanon.nc_cname = newstr(hp->h_name);
	if (bitset(MF_MATCHONLY, map->map_mflags))
		cp = map_rewrite(map, name, strlen(name), NULL);
	else
		cp = map_rewrite(map, hp->h_name, strlen(hp->h_name), av);
	return cp;
}

# else /* DAEMON */
/* code for systems without sophisticated networking */

/*
**  MYHOSTNAME -- stub version for case of no daemon code.
**
**	Can't convert to upper case here because might be a UUCP name.
**
**	Mark, you can change this to be anything you want......
*/

char **
myhostname(hostbuf, size)
	char hostbuf[];
	int size;
{
	register FILE *f;

	hostbuf[0] = '\0';
	f = fopen("/usr/include/whoami", "r");
	if (f != NULL)
	{
		(void) fgets(hostbuf, size, f);
		fixcrlf(hostbuf, TRUE);
		(void) fclose(f);
	}
	return (NULL);
}
/*
**  GETAUTHINFO -- get the real host name asociated with a file descriptor
**
**	Parameters:
**		fd -- the descriptor
**		may_be_forged -- an outage that is set to TRUE if the
**			forward lookup of RealHostName does not match
**			RealHostAddr; set to FALSE if they do match.
**
**	Returns:
**		The host name associated with this descriptor, if it can
**			be determined.
**		NULL otherwise.
**
**	Side Effects:
**		none
*/

char *
getauthinfo(fd, may_be_forged)
	int fd;
	bool *may_be_forged;
{
	*may_be_forged = FALSE;
	return NULL;
}
/*
**  MAPHOSTNAME -- turn a hostname into canonical form
**
**	Parameters:
**		map -- a pointer to the database map.
**		name -- a buffer containing a hostname.
**		avp -- a pointer to a (cf file defined) argument vector.
**		statp -- an exit status (out parameter).
**
**	Returns:
**		mapped host name
**		FALSE otherwise.
**
**	Side Effects:
**		Looks up the host specified in name.  If it is not
**		the canonical name for that host, replace it with
**		the canonical name.  If the name is unknown, or it
**		is already the canonical name, leave it unchanged.
*/

/*ARGSUSED*/
char *
host_map_lookup(map, name, avp, statp)
	MAP *map;
	char *name;
	char **avp;
	char *statp;
{
	register struct hostent *hp;
	char *cp;

	hp = sm_gethostbyname(name);
	if (hp == NULL)
	{
		*statp = EX_NOHOST;
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		cp = map_rewrite(map, name, strlen(name), NULL);
	else
		cp = map_rewrite(map, hp->h_name, strlen(hp->h_name), avp);
	return cp;
}

#endif /* DAEMON */
/*
**  HOST_MAP_INIT -- initialize host class structures
*/

bool
host_map_init(map, args)
	MAP *map;
	char *args;
{
	register char *p = args;

	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'a':
			map->map_app = ++p;
			break;

		  case 'T':
			map->map_tapp = ++p;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;
		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}
	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);
	if (map->map_tapp != NULL)
		map->map_tapp = newstr(map->map_tapp);
	return TRUE;
}
/*
**  ANYNET_NTOA -- convert a network address to printable form.
**
**	Parameters:
**		sap -- a pointer to a sockaddr structure.
**
**	Returns:
**		A printable version of that sockaddr.
*/

#ifdef USE_SOCK_STREAM

#if NETLINK
# include <net/if_dl.h>
#endif

char *
anynet_ntoa(sap)
	register SOCKADDR *sap;
{
	register char *bp;
	register char *ap;
	int l;
	static char buf[100];

	/* check for null/zero family */
	if (sap == NULL)
		return "NULLADDR";
	if (sap->sa.sa_family == 0)
		return "0";

	switch (sap->sa.sa_family)
	{
#if NETUNIX
	  case AF_UNIX:
	  	if (sap->sunix.sun_path[0] != '\0')
	  		snprintf(buf, sizeof buf, "[UNIX: %.64s]",
				sap->sunix.sun_path);
	  	else
	  		snprintf(buf, sizeof buf, "[UNIX: localhost]");
		return buf;
#endif

#if NETINET
	  case AF_INET:
		return inet_ntoa(sap->sin.sin_addr);
#endif

#if NETLINK
	  case AF_LINK:
		snprintf(buf, sizeof buf, "[LINK: %s]",
			link_ntoa((struct sockaddr_dl *) &sap->sa));
		return buf;
#endif
	  default:
		/* this case is needed when nothing is #defined */
		/* in order to keep the switch syntactically correct */
		break;
	}

	/* unknown family -- just dump bytes */
	(void) snprintf(buf, sizeof buf, "Family %d: ", sap->sa.sa_family);
	bp = &buf[strlen(buf)];
	ap = sap->sa.sa_data;
	for (l = sizeof sap->sa.sa_data; --l >= 0; )
	{
		(void) snprintf(bp, SPACELEFT(buf, bp), "%02x:", *ap++ & 0377);
		bp += 3;
	}
	*--bp = '\0';
	return buf;
}
/*
**  HOSTNAMEBYANYADDR -- return name of host based on address
**
**	Parameters:
**		sap -- SOCKADDR pointer
**
**	Returns:
**		text representation of host name.
**
**	Side Effects:
**		none.
*/

char *
hostnamebyanyaddr(sap)
	register SOCKADDR *sap;
{
	register struct hostent *hp;
	int saveretry;

#if NAMED_BIND
	/* shorten name server timeout to avoid higher level timeouts */
	saveretry = _res.retry;
	_res.retry = 3;
#endif /* NAMED_BIND */

	switch (sap->sa.sa_family)
	{
#if NETINET
	  case AF_INET:
		hp = sm_gethostbyaddr((char *) &sap->sin.sin_addr,
			INADDRSZ,
			AF_INET);
		break;
#endif

#if NETISO
	  case AF_ISO:
		hp = sm_gethostbyaddr((char *) &sap->siso.siso_addr,
			sizeof sap->siso.siso_addr,
			AF_ISO);
		break;
#endif

#if NETUNIX
	  case AF_UNIX:
		hp = NULL;
		break;
#endif

	  default:
		hp = sm_gethostbyaddr(sap->sa.sa_data,
			   sizeof sap->sa.sa_data,
			   sap->sa.sa_family);
		break;
	}

#if NAMED_BIND
	_res.retry = saveretry;
#endif /* NAMED_BIND */

	if (hp != NULL && hp->h_name[0] != '[' &&
	    inet_addr(hp->h_name) == INADDR_NONE)
		return denlstring((char *) hp->h_name, TRUE, TRUE);
#if NETUNIX
	else if (sap->sa.sa_family == AF_UNIX && sap->sunix.sun_path[0] == '\0')
		return "localhost";
#endif
	else
	{
		/* produce a dotted quad */
		static char buf[203];

		(void) snprintf(buf, sizeof buf, "[%.200s]", anynet_ntoa(sap));
		return buf;
	}
}

#endif /* SOCK_STREAM */
