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

#include <errno.h>
#include <signal.h>
#include "sendmail.h"

#ifndef lint
#ifdef DAEMON
static char sccsid[] = "@(#)daemon.c	8.2 (Berkeley) 7/13/93 (with daemon mode)";
#else
static char sccsid[] = "@(#)daemon.c	8.2 (Berkeley) 7/13/93 (without daemon mode)";
#endif
#endif /* not lint */

#ifdef DAEMON

# include <netdb.h>
# include <sys/wait.h>
# include <sys/time.h>

#ifdef NAMED_BIND
# include <arpa/nameser.h>
# include <resolv.h>
#endif

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
**	getrequests()
**		Opens a port and initiates a connection.
**		Returns in a child.  Must set InChannel and
**		OutChannel appropriately.
**	clrdaemon()
**		Close any open files associated with getting
**		the connection; this is used when running the queue,
**		etc., to avoid having extra file descriptors during
**		the queue run and to avoid confusing the network
**		code (if it cares).
**	makeconnection(host, port, outfile, infile, usesecureport)
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
**		none.
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

getrequests()
{
	int t;
	register struct servent *sp;
	int on = 1;
	bool refusingconnections = TRUE;
	FILE *pidf;
	extern void reapchild();

	/*
	**  Set up the address for the mailer.
	*/

	if (DaemonAddr.sin.sin_family == 0)
		DaemonAddr.sin.sin_family = AF_INET;
	if (DaemonAddr.sin.sin_addr.s_addr == 0)
		DaemonAddr.sin.sin_addr.s_addr = INADDR_ANY;
	if (DaemonAddr.sin.sin_port == 0)
	{
		sp = getservbyname("smtp", "tcp");
		if (sp == NULL)
		{
			syserr("554 service \"smtp\" unknown");
			goto severe;
		}
#ifdef _SCO_unix_
		DaemonAddr.sin.sin_port = htons(sp->s_port);
#else
		DaemonAddr.sin.sin_port = sp->s_port;
#endif
	}

	/*
	**  Try to actually open the connection.
	*/

	if (tTd(15, 1))
		printf("getrequests: port 0x%x\n", DaemonAddr.sin.sin_port);

	/* get a socket for the SMTP connection */
	DaemonSocket = socket(DaemonAddr.sa.sa_family, SOCK_STREAM, 0);
	if (DaemonSocket < 0)
	{
		/* probably another daemon already */
		syserr("getrequests: can't create socket");
	  severe:
# ifdef LOG
		if (LogLevel > 0)
			syslog(LOG_ALERT, "problem creating SMTP socket");
# endif /* LOG */
		finis();
	}

	/* turn on network debugging? */
	if (tTd(15, 101))
		(void) setsockopt(DaemonSocket, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof on);

	(void) setsockopt(DaemonSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof on);
	(void) setsockopt(DaemonSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof on);

	switch (DaemonAddr.sa.sa_family)
	{
# ifdef NETINET
	  case AF_INET:
		t = sizeof DaemonAddr.sin;
		break;
# endif

# ifdef NETISO
	  case AF_ISO:
		t = sizeof DaemonAddr.siso;
		break;
# endif

	  default:
		t = sizeof DaemonAddr;
		break;
	}

	if (bind(DaemonSocket, &DaemonAddr.sa, t) < 0)
	{
		syserr("getrequests: cannot bind");
		(void) close(DaemonSocket);
		goto severe;
	}

	(void) signal(SIGCHLD, reapchild);

	/* write the pid to the log file for posterity */
	pidf = fopen(PidFile, "w");
	if (pidf != NULL)
	{
		fprintf(pidf, "%d\n", getpid());
		fclose(pidf);
	}


	if (tTd(15, 1))
		printf("getrequests: %d\n", DaemonSocket);

	for (;;)
	{
		register int pid;
		auto int lotherend;
		extern bool refuseconnections();

		/* see if we are rejecting connections */
		CurrentLA = getla();
		if (refuseconnections())
		{
			if (!refusingconnections)
			{
				/* don't queue so peer will fail quickly */
				(void) listen(DaemonSocket, 0);
				refusingconnections = TRUE;
			}
			setproctitle("rejecting connections: load average: %d",
				CurrentLA);
			sleep(5);
			continue;
		}

		if (refusingconnections)
		{
			/* start listening again */
			if (listen(DaemonSocket, ListenQueueSize) < 0)
			{
				syserr("getrequests: cannot listen");
				(void) close(DaemonSocket);
				goto severe;
			}
			setproctitle("accepting connections");
			refusingconnections = FALSE;
		}

		/* wait for a connection */
		do
		{
			errno = 0;
			lotherend = sizeof RealHostAddr;
			t = accept(DaemonSocket,
			    (struct sockaddr *)&RealHostAddr, &lotherend);
		} while (t < 0 && errno == EINTR);
		if (t < 0)
		{
			syserr("getrequests: accept");
			sleep(5);
			continue;
		}

		/*
		**  Create a subprocess to process the mail.
		*/

		if (tTd(15, 2))
			printf("getrequests: forking (fd = %d)\n", t);

		pid = fork();
		if (pid < 0)
		{
			syserr("daemon: cannot fork");
			sleep(10);
			(void) close(t);
			continue;
		}

		if (pid == 0)
		{
			extern char *hostnamebyanyaddr();

			/*
			**  CHILD -- return to caller.
			**	Collect verified idea of sending host.
			**	Verify calling user id if possible here.
			*/

			(void) signal(SIGCHLD, SIG_DFL);
			OpMode = MD_SMTP;

			/* determine host name */
			RealHostName = newstr(hostnamebyanyaddr(&RealHostAddr));

#ifdef LOG
			if (LogLevel > 10)
			{
				/* log connection information */
				syslog(LOG_INFO, "connect from %s (%s)",
					RealHostName, anynet_ntoa(&RealHostAddr));
			}
#endif

			(void) close(DaemonSocket);
			InChannel = fdopen(t, "r");
			OutChannel = fdopen(dup(t), "w");

			/* should we check for illegal connection here? XXX */
#ifdef XLA
			if (!xla_host_ok(RealHostName))
			{
				message("421 Too many SMTP sessions for this host");
				exit(0);
			}
#endif

			if (tTd(15, 2))
				printf("getreq: returning\n");
			return;
		}

		/* close the port so that others will hang (for a while) */
		(void) close(t);
	}
	/*NOTREACHED*/
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

		switch (*f)
		{
		  case 'F':		/* address family */
			if (isascii(*v) && isdigit(*v))
				DaemonAddr.sa.sa_family = atoi(v);
#ifdef NETINET
			else if (strcasecmp(v, "inet") == 0)
				DaemonAddr.sa.sa_family = AF_INET;
#endif
#ifdef NETISO
			else if (strcasecmp(v, "iso") == 0)
				DaemonAddr.sa.sa_family = AF_ISO;
#endif
#ifdef NETNS
			else if (strcasecmp(v, "ns") == 0)
				DaemonAddr.sa.sa_family = AF_NS;
#endif
#ifdef NETX25
			else if (strcasecmp(v, "x.25") == 0)
				DaemonAddr.sa.sa_family = AF_CCITT;
#endif
			else
				syserr("554 Unknown address family %s in Family=option", v);
			break;

		  case 'A':		/* address */
			switch (DaemonAddr.sa.sa_family)
			{
#ifdef NETINET
			  case AF_INET:
				if (isascii(*v) && isdigit(*v))
					DaemonAddr.sin.sin_addr.s_addr = inet_network(v);
				else
				{
					register struct netent *np;

					np = getnetbyname(v);
					if (np == NULL)
						syserr("554 network \"%s\" unknown", v);
					else
						DaemonAddr.sin.sin_addr.s_addr = np->n_net;
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
				short port;

#ifdef NETINET
			  case AF_INET:
				if (isascii(*v) && isdigit(*v))
					DaemonAddr.sin.sin_port = atoi(v);
				else
				{
					register struct servent *sp;

					sp = getservbyname(v, "tcp");
					if (sp == NULL)
						syserr("554 service \"%s\" unknown", v);
					else
					{
#ifdef _SCO_unix_
						DaemonAddr.sin.sin_port = htons(sp->s_port);
#else
						DaemonAddr.sin.sin_port = sp->s_port;
#endif
					}
				}
				break;
#endif

#ifdef NETISO
			  case AF_ISO:
				/* assume two byte transport selector */
				if (isascii(*v) && isdigit(*v))
					port = atoi(v);
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
**		usesecureport -- if set, use a low numbered (reserved)
**			port to provide some rudimentary authentication.
**
**	Returns:
**		An exit code telling whether the connection could be
**			made and if not why not.
**
**	Side Effects:
**		none.
*/

SOCKADDR	CurHostAddr;		/* address of current host */

int
makeconnection(host, port, mci, usesecureport)
	char *host;
	u_short port;
	register MCI *mci;
	bool usesecureport;
{
	register int i, s;
	register struct hostent *hp = (struct hostent *)NULL;
	SOCKADDR addr;
	int sav_errno;
	int addrlen;
#ifdef NAMED_BIND
	extern int h_errno;
#endif

	/*
	**  Set up the address for the mailer.
	**	Accept "[a.b.c.d]" syntax for host name.
	*/

#ifdef NAMED_BIND
	h_errno = 0;
#endif
	errno = 0;
	bzero(&CurHostAddr, sizeof CurHostAddr);
	CurHostName = host;

	if (host[0] == '[')
	{
		long hid;
		register char *p = strchr(host, ']');

		if (p != NULL)
		{
			*p = '\0';
#ifdef NETINET
			hid = inet_addr(&host[1]);
			if (hid == -1)
#endif
			{
				/* try it as a host name (avoid MX lookup) */
				hp = gethostbyname(&host[1]);
				*p = ']';
				goto gothostent;
			}
			*p = ']';
		}
		if (p == NULL)
		{
			usrerr("553 Invalid numeric domain spec \"%s\"", host);
			return (EX_NOHOST);
		}
#ifdef NETINET
		addr.sin.sin_family = AF_INET;		/*XXX*/
		addr.sin.sin_addr.s_addr = hid;
#endif
	}
	else
	{
		hp = gethostbyname(host);
gothostent:
		if (hp == NULL)
		{
#ifdef NAMED_BIND
			if (errno == ETIMEDOUT || h_errno == TRY_AGAIN)
				return (EX_TEMPFAIL);

			/* if name server is specified, assume temp fail */
			if (errno == ECONNREFUSED && UseNameServer)
				return (EX_TEMPFAIL);
#endif
			return (EX_NOHOST);
		}
		addr.sa.sa_family = hp->h_addrtype;
		switch (hp->h_addrtype)
		{
#ifdef NETINET
		  case AF_INET:
			bcopy(hp->h_addr,
				&addr.sin.sin_addr,
				hp->h_length);
			break;
#endif

		  default:
			bcopy(hp->h_addr,
				addr.sa.sa_data,
				hp->h_length);
			break;
		}
		i = 1;
	}

	/*
	**  Determine the port number.
	*/

	if (port != 0)
		port = htons(port);
	else
	{
		register struct servent *sp = getservbyname("smtp", "tcp");

		if (sp == NULL)
		{
			syserr("554 makeconnection: service \"smtp\" unknown");
			return (EX_OSERR);
		}
#ifdef _SCO_unix_
		port = htons(sp->s_port);
#else
		port = sp->s_port;
#endif
	}

	switch (addr.sa.sa_family)
	{
#ifdef NETINET
	  case AF_INET:
		addr.sin.sin_port = port;
		addrlen = sizeof (struct sockaddr_in);
		break;
#endif

#ifdef NETISO
	  case AF_ISO:
		/* assume two byte transport selector */
		bcopy((char *) &port, TSEL((struct sockaddr_iso *) &addr), 2);
		addrlen = sizeof (struct sockaddr_iso);
		break;
#endif

	  default:
		syserr("Can't connect to address family %d", addr.sa.sa_family);
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

	for (;;)
	{
		if (tTd(16, 1))
			printf("makeconnection (%s [%s])\n",
				host, anynet_ntoa(&addr));

		/* save for logging */
		CurHostAddr = addr;

		if (usesecureport)
		{
			int rport = IPPORT_RESERVED - 1;

			s = rresvport(&rport);
		}
		else
		{
			s = socket(AF_INET, SOCK_STREAM, 0);
		}
		if (s < 0)
		{
			sav_errno = errno;
			syserr("makeconnection: no socket");
			goto failure;
		}

		if (tTd(16, 1))
			printf("makeconnection: fd=%d\n", s);

		/* turn on network debugging? */
		if (tTd(16, 101))
		{
			int on = 1;
			(void) setsockopt(DaemonSocket, SOL_SOCKET, SO_DEBUG,
					  (char *)&on, sizeof on);
		}
		if (CurEnv->e_xfp != NULL)
			(void) fflush(CurEnv->e_xfp);		/* for debugging */
		errno = 0;					/* for debugging */
		if (connect(s, (struct sockaddr *) &addr, addrlen) >= 0)
			break;

		/* couldn't connect.... figure out why */
		sav_errno = errno;
		(void) close(s);
		if (hp && hp->h_addr_list[i])
		{
			if (tTd(16, 1))
				printf("Connect failed (%s); trying new address....\n",
					errstring(sav_errno));
			switch (addr.sa.sa_family)
			{
#ifdef NETINET
			  case AF_INET:
				bcopy(hp->h_addr_list[i++],
				      &addr.sin.sin_addr,
				      hp->h_length);
				break;
#endif

			  default:
				bcopy(hp->h_addr_list[i++],
					addr.sa.sa_data,
					hp->h_length);
				break;
			}
			continue;
		}

		/* failure, decide if temporary or not */
	failure:
#ifdef XLA
		xla_host_end(host);
#endif
		if (transienterror(sav_errno))
			return EX_TEMPFAIL;
		else
		{
			message("%s", errstring(sav_errno));
			return (EX_UNAVAILABLE);
		}
	}

	/* connection ok, put it into canonical form */
	mci->mci_out = fdopen(s, "w");
	mci->mci_in = fdopen(dup(s), "r");

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
**		Sets the MyIpAddrs buffer to a list of my IP addresses.
*/

struct in_addr	MyIpAddrs[MAXIPADDR + 1];

char **
myhostname(hostbuf, size)
	char hostbuf[];
	int size;
{
	register struct hostent *hp;
	extern struct hostent *gethostbyname();

	if (gethostname(hostbuf, size) < 0)
	{
		(void) strcpy(hostbuf, "localhost");
	}
	hp = gethostbyname(hostbuf);
	if (hp != NULL)
	{
		(void) strncpy(hostbuf, hp->h_name, size - 1);
		hostbuf[size - 1] = '\0';

		if (hp->h_addrtype == AF_INET && hp->h_length == 4)
		{
			register int i;

			for (i = 0; i < MAXIPADDR; i++)
			{
				if (hp->h_addr_list[i] == NULL)
					break;
				MyIpAddrs[i].s_addr = *(u_long *) hp->h_addr_list[i];
			}
			MyIpAddrs[i].s_addr = 0;
		}

		return (hp->h_aliases);
	}
	else
		return (NULL);
}
/*
**  GETAUTHINFO -- get the real host name asociated with a file descriptor
**
**	Uses RFC1413 protocol to try to get info from the other end.
**
**	Parameters:
**		fd -- the descriptor
**
**	Returns:
**		The user@host information associated with this descriptor.
**
**	Side Effects:
**		Sets RealHostName to the name of the host at the other end.
*/

#ifdef IDENTPROTO

static jmp_buf	CtxAuthTimeout;

static
authtimeout()
{
	longjmp(CtxAuthTimeout, 1);
}

#endif

char *
getauthinfo(fd)
	int fd;
{
	SOCKADDR fa;
	int falen;
	register char *p;
#ifdef IDENTPROTO
	SOCKADDR la;
	int lalen;
	register struct servent *sp;
	int s;
	int i;
	EVENT *ev;
#endif
	static char hbuf[MAXNAME * 2 + 2];
	extern char *hostnamebyanyaddr();
	extern char RealUserName[];			/* main.c */

	falen = sizeof fa;
	if (getpeername(fd, &fa.sa, &falen) < 0 || falen <= 0)
	{
		RealHostName = "localhost";
		(void) sprintf(hbuf, "%s@localhost", RealUserName);
		if (tTd(9, 1))
			printf("getauthinfo: %s\n", hbuf);
		return hbuf;
	}

	RealHostName = newstr(hostnamebyanyaddr(&fa));
	RealHostAddr = fa;

#ifdef IDENTPROTO
	lalen = sizeof la;
	if (fa.sa.sa_family != AF_INET ||
	    getsockname(fd, &la.sa, &lalen) < 0 || lalen <= 0 ||
	    la.sa.sa_family != AF_INET)
	{
		/* no ident info */
		goto noident;
	}

	/* create ident query */
	(void) sprintf(hbuf, "%d,%d\r\n",
		ntohs(fa.sin.sin_port), ntohs(la.sin.sin_port));

	/* create local address */
	bzero(&la, sizeof la);

	/* create foreign address */
	sp = getservbyname("auth", "tcp");
	if (sp != NULL)
	{
#ifdef _SCO_unix_
		fa.sin.sin_port = htons(sp->s_port);
#else
		fa.sin.sin_port = sp->s_port;
#endif
	}
	else
		fa.sin.sin_port = htons(113);

	s = -1;
	if (setjmp(CtxAuthTimeout) != 0)
	{
		if (s >= 0)
			(void) close(s);
		goto noident;
	}

	/* put a timeout around the whole thing */
	ev = setevent((time_t) 30, authtimeout, 0);

	/* connect to foreign IDENT server */
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		clrevent(ev);
		goto noident;
	}
	if (connect(s, &fa.sa, sizeof fa.sin) < 0)
	{
closeident:
		(void) close(s);
		clrevent(ev);
		goto noident;
	}

	if (tTd(9, 10))
		printf("getauthinfo: sent %s", hbuf);

	/* send query */
	if (write(s, hbuf, strlen(hbuf)) < 0)
		goto closeident;

	/* get result */
	i = read(s, hbuf, sizeof hbuf);
	(void) close(s);
	clrevent(ev);
	if (i <= 0)
		goto noident;
	if (hbuf[--i] == '\n' && hbuf[--i] == '\r')
		i--;
	hbuf[++i] = '\0';

	if (tTd(9, 3))
		printf("getauthinfo:  got %s\n", hbuf);

	/* parse result */
	p = strchr(hbuf, ':');
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
	p = strchr(p, ':');
	if (p == NULL)
	{
		/* malformed response */
		goto noident;
	}

	/* 1413 says don't do this -- but it's broken otherwise */
	while (isascii(*++p) && isspace(*p))
		continue;

	/* p now points to the authenticated name */
	(void) sprintf(hbuf, "%s@%s", p, RealHostName);
	goto finish;

#endif /* IDENTPROTO */

noident:
	(void) strcpy(hbuf, RealHostName);

finish:
	if (RealHostName[0] != '[')
	{
		p = &hbuf[strlen(hbuf)];
		(void) sprintf(p, " [%s]", anynet_ntoa(&RealHostAddr));
	}
	if (tTd(9, 1))
		printf("getauthinfo: %s\n", hbuf);
	return hbuf;
}
/*
**  HOST_MAP_LOOKUP -- turn a hostname into canonical form
**
**	Parameters:
**		map -- a pointer to this map (unused).
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
**		name.
*/

char *
host_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	register struct hostent *hp;
	u_long in_addr;
	char *cp;
	int i;
	register STAB *s;
	char hbuf[MAXNAME];
	extern struct hostent *gethostbyaddr();
	extern int h_errno;

	/*
	**  See if we have already looked up this name.  If so, just
	**  return it.
	*/

	s = stab(name, ST_NAMECANON, ST_ENTER);
	if (bitset(NCF_VALID, s->s_namecanon.nc_flags))
	{
		if (tTd(9, 1))
			printf("host_map_lookup(%s) => CACHE %s\n",
				name, s->s_namecanon.nc_cname);
		errno = s->s_namecanon.nc_errno;
		h_errno = s->s_namecanon.nc_herrno;
		*statp = s->s_namecanon.nc_stat;
		return s->s_namecanon.nc_cname;
	}

	/*
	**  If first character is a bracket, then it is an address
	**  lookup.  Address is copied into a temporary buffer to
	**  strip the brackets and to preserve name if address is
	**  unknown.
	*/

	if (*name != '[')
	{
		extern bool getcanonname();

		if (tTd(9, 1))
			printf("host_map_lookup(%s) => ", name);
		s->s_namecanon.nc_flags |= NCF_VALID;		/* will be soon */
		(void) strcpy(hbuf, name);
		if (getcanonname(hbuf, sizeof hbuf - 1))
		{
			if (tTd(9, 1))
				printf("%s\n", hbuf);
			cp = map_rewrite(map, hbuf, strlen(hbuf), av);
			s->s_namecanon.nc_cname = newstr(cp);
			return cp;
		}
		else
		{
			register struct hostent *hp;

			if (tTd(9, 1))
				printf("FAIL (%d)\n", h_errno);
			s->s_namecanon.nc_errno = errno;
			s->s_namecanon.nc_herrno = h_errno;
			switch (h_errno)
			{
			  case TRY_AGAIN:
				if (UseNameServer)
				{
					char *msg = "Recipient domain nameserver timed out";

					message(msg);
					if (CurEnv->e_message == NULL)
						CurEnv->e_message = newstr(msg);
				}
				*statp = EX_TEMPFAIL;
				break;

			  case HOST_NOT_FOUND:
				*statp = EX_NOHOST;
				break;

			  case NO_RECOVERY:
				*statp = EX_SOFTWARE;
				break;

			  default:
				*statp = EX_UNAVAILABLE;
				break;
			}
			s->s_namecanon.nc_stat = *statp;
			if (*statp != EX_TEMPFAIL || UseNameServer)
				return NULL;

			/*
			**  Try to look it up in /etc/hosts
			*/

			hp = gethostbyname(name);
			if (hp == NULL)
			{
				/* no dice there either */
				s->s_namecanon.nc_stat = *statp = EX_NOHOST;
				return NULL;
			}

			s->s_namecanon.nc_stat = *statp = EX_OK;
			cp = map_rewrite(map, hp->h_name, strlen(hp->h_name), av);
			s->s_namecanon.nc_cname = newstr(cp);
			return cp;
		}
	}
	if ((cp = strchr(name, ']')) == NULL)
		return (NULL);
	*cp = '\0';
	in_addr = inet_addr(&name[1]);

	/* check to see if this is one of our addresses */
	for (i = 0; MyIpAddrs[i].s_addr != 0; i++)
	{
		if (MyIpAddrs[i].s_addr == in_addr)
		{
			return map_rewrite(map, MyHostName, strlen(MyHostName), av);
		}
	}

	/* nope -- ask the name server */
	hp = gethostbyaddr((char *)&in_addr, sizeof(struct in_addr), AF_INET);
	s->s_namecanon.nc_errno = errno;
	s->s_namecanon.nc_herrno = h_errno;
	s->s_namecanon.nc_flags |= NCF_VALID;		/* will be soon */
	if (hp == NULL)
	{
		s->s_namecanon.nc_stat = *statp = EX_NOHOST;
		return (NULL);
	}

	/* found a match -- copy out */
	cp = map_rewrite(map, hp->h_name, strlen(hp->h_name), av);
	s->s_namecanon.nc_stat = *statp = EX_OK;
	s->s_namecanon.nc_cname = newstr(cp);
	return cp;
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

char *
anynet_ntoa(sap)
	register SOCKADDR *sap;
{
	register char *bp;
	register char *ap;
	int l;
	static char buf[80];

	/* check for null/zero family */
	if (sap == NULL)
		return "NULLADDR";
	if (sap->sa.sa_family == 0)
		return "0";

#ifdef NETINET
	if (sap->sa.sa_family == AF_INET)
	{
		extern char *inet_ntoa();

		return inet_ntoa(((struct sockaddr_in *) sap)->sin_addr);
	}
#endif

	/* unknown family -- just dump bytes */
	(void) sprintf(buf, "Family %d: ", sap->sa.sa_family);
	bp = &buf[strlen(buf)];
	ap = sap->sa.sa_data;
	for (l = sizeof sap->sa.sa_data; --l >= 0; )
	{
		(void) sprintf(bp, "%02x:", *ap++ & 0377);
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

#ifdef NAMED_BIND
	int saveretry;

	/* shorten name server timeout to avoid higher level timeouts */
	saveretry = _res.retry;
	_res.retry = 3;
#endif /* NAMED_BIND */

	switch (sap->sa.sa_family)
	{
#ifdef NETINET
	  case AF_INET:
		hp = gethostbyaddr((char *) &sap->sin.sin_addr,
			sizeof sap->sin.sin_addr,
			AF_INET);
		break;
#endif

#ifdef NETISO
	  case AF_ISO:
		hp = gethostbyaddr((char *) &sap->siso.siso_addr,
			sizeof sap->siso.siso_addr,
			AF_ISO);
		break;
#endif

	  default:
		hp = gethostbyaddr(sap->sa.sa_data,
			   sizeof sap->sa.sa_data,
			   sap->sa.sa_family);
		break;
	}

#ifdef NAMED_BIND
	_res.retry = saveretry;
#endif /* NAMED_BIND */

	if (hp != NULL)
		return hp->h_name;
	else
	{
		/* produce a dotted quad */
		static char buf[512];

		(void) sprintf(buf, "[%s]", anynet_ntoa(sap));
		return buf;
	}
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
getauthinfo(fd)
	int fd;
{
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

	hp = gethostbyname(name);
	if (hp != NULL)
		return hp->h_name;
	*statp = EX_NOHOST;
	return NULL;
}

#endif /* DAEMON */
