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
# ifdef DAEMON
static char id[] = "@(#)$Id: daemon.c,v 8.401.4.61 2001/05/27 22:14:40 gshapiro Exp $ (with daemon mode)";
# else /* DAEMON */
static char id[] = "@(#)$Id: daemon.c,v 8.401.4.61 2001/05/27 22:14:40 gshapiro Exp $ (without daemon mode)";
# endif /* DAEMON */
#endif /* ! lint */

#if defined(SOCK_STREAM) || defined(__GNU_LIBRARY__)
# define USE_SOCK_STREAM	1
#endif /* defined(SOCK_STREAM) || defined(__GNU_LIBRARY__) */

#if DAEMON || defined(USE_SOCK_STREAM)
# if NETINET || NETINET6
#  include <arpa/inet.h>
# endif /* NETINET || NETINET6 */
# if NAMED_BIND
#  ifndef NO_DATA
#   define NO_DATA	NO_ADDRESS
#  endif /* ! NO_DATA */
# endif /* NAMED_BIND */
#endif /* DAEMON || defined(USE_SOCK_STREAM) */

#if DAEMON

# if STARTTLS
#    include <openssl/rand.h>
# endif /* STARTTLS */

# include <sys/time.h>

# if IP_SRCROUTE && NETINET
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
#  if HAS_IN_H
#   include <netinet/in.h>
#   ifndef IPOPTION
#    define IPOPTION	ip_opts
#    define IP_LIST	ip_opts
#    define IP_DST	ip_dst
#   endif /* ! IPOPTION */
#  else /* HAS_IN_H */
#   include <netinet/ip_var.h>
#   ifndef IPOPTION
#    define IPOPTION	ipoption
#    define IP_LIST	ipopt_list
#    define IP_DST	ipopt_dst
#   endif /* ! IPOPTION */
#  endif /* HAS_IN_H */
# endif /* IP_SRCROUTE && NETINET */

/* structure to describe a daemon */
struct daemon
{
	int		d_socket;	/* fd for socket */
	SOCKADDR	d_addr;		/* socket for incoming */
	u_short		d_port;		/* port number */
	int		d_listenqueue;	/* size of listen queue */
	int		d_tcprcvbufsize;	/* size of TCP receive buffer */
	int		d_tcpsndbufsize;	/* size of TCP send buffer */
	time_t		d_refuse_connections_until;
	bool		d_firsttime;
	int		d_socksize;
	BITMAP256	d_flags;	/* flags; see sendmail.h */
	char		*d_mflags;	/* flags for use in macro */
	char		*d_name;	/* user-supplied name */
};

typedef struct daemon DAEMON_T;

static void	connecttimeout __P((void));
static int	opendaemonsocket __P((struct daemon *, bool));
static u_short	setupdaemon __P((SOCKADDR *));
static SIGFUNC_DECL	sighup __P((int));
static void	restart_daemon __P((void));

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

static DAEMON_T	Daemons[MAXDAEMONS];
static int	ndaemons = 0;			/* actual number of daemons */

/* options for client */
static int	TcpRcvBufferSize = 0;	/* size of TCP receive buffer */
static int	TcpSndBufferSize = 0;	/* size of TCP send buffer */

/*
**  GETREQUESTS -- open mail IPC port and get requests.
**
**	Parameters:
**		e -- the current envelope.
**
**	Returns:
**		pointer to flags.
**
**	Side Effects:
**		Waits until some interesting activity occurs.  When
**		it does, a child is created to process it, and the
**		parent waits for completion.  Return from this
**		routine is always in the child.  The file pointers
**		"InChannel" and "OutChannel" should be set to point
**		to the communication channel.
*/

BITMAP256 *
getrequests(e)
	ENVELOPE *e;
{
	int t;
	time_t last_disk_space_check = 0;
	int idx, curdaemon = -1;
	int i, olddaemon = 0;
# if XDEBUG
	bool j_has_dot;
# endif /* XDEBUG */
	char status[MAXLINE];
	SOCKADDR sa;
	SOCKADDR_LEN_T len = sizeof sa;
# if NETUNIX
	extern int ControlSocket;
# endif /* NETUNIX */
	extern ENVELOPE BlankEnvelope;


	for (idx = 0; idx < ndaemons; idx++)
	{
		Daemons[idx].d_port = setupdaemon(&(Daemons[idx].d_addr));
		Daemons[idx].d_firsttime = TRUE;
		Daemons[idx].d_refuse_connections_until = (time_t) 0;
	}

	/*
	**  Try to actually open the connection.
	*/

	if (tTd(15, 1))
	{
		for (idx = 0; idx < ndaemons; idx++)
		{
			dprintf("getrequests: daemon %s: port %d\n",
				Daemons[idx].d_name,
				ntohs(Daemons[idx].d_port));
		}
	}

	/* get a socket for the SMTP connection */
	for (idx = 0; idx < ndaemons; idx++)
		Daemons[idx].d_socksize = opendaemonsocket(&Daemons[idx], TRUE);

	if (opencontrolsocket() < 0)
		sm_syslog(LOG_WARNING, NOQID,
			  "daemon could not open control socket %s: %s",
			  ControlSocketName, errstring(errno));

	(void) setsignal(SIGCHLD, reapchild);
	(void) setsignal(SIGHUP, sighup);

	/* workaround: can't seem to release the signal in the parent */
	(void) releasesignal(SIGHUP);

	/* write the pid to file */
	log_sendmail_pid(e);

# if XDEBUG
	{
		char jbuf[MAXHOSTNAMELEN];

		expand("\201j", jbuf, sizeof jbuf, e);
		j_has_dot = strchr(jbuf, '.') != NULL;
	}
# endif /* XDEBUG */

	/* Add parent process as first item */
	proc_list_add(getpid(), "Sendmail daemon", PROC_DAEMON);

	if (tTd(15, 1))
	{
		for (idx = 0; idx < ndaemons; idx++)
			dprintf("getrequests: daemon %s: %d\n",
				Daemons[idx].d_name,
				Daemons[idx].d_socket);
	}

	for (;;)
	{
		register pid_t pid;
		auto SOCKADDR_LEN_T lotherend;
		bool timedout = FALSE;
		bool control = FALSE;
		int save_errno;
		int pipefd[2];
		time_t timenow;
# if STARTTLS
		long seed;
# endif /* STARTTLS */
		extern bool refuseconnections __P((char *, ENVELOPE *, int));

		/* see if we are rejecting connections */
		(void) blocksignal(SIGALRM);

		if (ShutdownRequest != NULL)
			shutdown_daemon();
		else if (RestartRequest != NULL)
			restart_daemon();

		timenow = curtime();

		/*
		**  Use ConnRateThrottle only if the
		**  last pass was for a connection
		*/

		if (ConnRateThrottle > 0 && curdaemon >= 0)
		{
			static int conncnt = 0;
			static time_t lastconn = 0;

			if (timenow != lastconn)
			{
				lastconn = timenow;
				conncnt = 1;
			}
			else if (++conncnt > ConnRateThrottle)
			{
				/* sleep to flatten out connection load */
				sm_setproctitle(TRUE, e,
						"deferring connections: %d per second",
						ConnRateThrottle);
				if (LogLevel >= 9)
					sm_syslog(LOG_INFO, NOQID,
						  "deferring connections: %d per second",
						  ConnRateThrottle);
				(void) sleep(1);
			}
		}

		for (idx = 0; idx < ndaemons; idx++)
		{
			if (timenow < Daemons[idx].d_refuse_connections_until)
				continue;
			if (refuseconnections(Daemons[idx].d_name, e, idx))
			{
				if (Daemons[idx].d_socket >= 0)
				{
					/* close socket so peer fails quickly */
					(void) close(Daemons[idx].d_socket);
					Daemons[idx].d_socket = -1;
				}

				/* refuse connections for next 15 seconds */
				Daemons[idx].d_refuse_connections_until = timenow + 15;
			}
			else if (Daemons[idx].d_socket < 0 ||
				 Daemons[idx].d_firsttime)
			{
				if (!Daemons[idx].d_firsttime && LogLevel >= 9)
					sm_syslog(LOG_INFO, NOQID,
						"accepting connections again for daemon %s",
						Daemons[idx].d_name);

				/* arrange to (re)open the socket if needed */
				(void) opendaemonsocket(&Daemons[idx], FALSE);
				Daemons[idx].d_firsttime = FALSE;
			}
		}

		/* May have been sleeping above, check again */
		if (ShutdownRequest != NULL)
			shutdown_daemon();
		else if (RestartRequest != NULL)
			restart_daemon();

		if (timenow >= last_disk_space_check)
		{
			bool logged = FALSE;

			if (!enoughdiskspace(MinBlocksFree + 1, FALSE))
			{
				for (idx = 0; idx < ndaemons; idx++)
				{
					if (!bitnset(D_ETRNONLY, Daemons[idx].d_flags))
					{
						/* log only if not logged before */
						if (!logged)
						{
							if (LogLevel >= 9)
								sm_syslog(LOG_INFO, NOQID,
									  "rejecting new messages: min free: %ld",
									  MinBlocksFree);
							logged = TRUE;
							sm_setproctitle(TRUE, e,
									"rejecting new messages: min free: %ld",
									MinBlocksFree);
						}
						setbitn(D_ETRNONLY, Daemons[idx].d_flags);
					}
				}
			}
			else
			{
				for (idx = 0; idx < ndaemons; idx++)
				{
					if (bitnset(D_ETRNONLY, Daemons[idx].d_flags))
					{
						/* log only if not logged before */
						if (!logged)
						{
							if (LogLevel >= 9)
								sm_syslog(LOG_INFO, NOQID,
									  "accepting new messages (again)");
							logged = TRUE;
						}

						/* title will be set below */
						clrbitn(D_ETRNONLY, Daemons[idx].d_flags);
					}
				}
			}
			/* only check disk space once a minute */
			last_disk_space_check = timenow + 60;
		}

# if XDEBUG
		/* check for disaster */
		{
			char jbuf[MAXHOSTNAMELEN];

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
# endif /* XDEBUG */

# if 0
		/*
		**  Andrew Sun <asun@ieps-sun.ml.com> claims that this will
		**  fix the SVr4 problem.  But it seems to have gone away,
		**  so is it worth doing this?
		*/

		if (DaemonSocket >= 0 &&
		    SetNonBlocking(DaemonSocket, FALSE) < 0)
			log an error here;
# endif /* 0 */
		(void) releasesignal(SIGALRM);

		for (;;)
		{
			bool setproc = FALSE;
			int highest = -1;
			fd_set readfds;
			struct timeval timeout;

			if (ShutdownRequest != NULL)
				shutdown_daemon();
			else if (RestartRequest != NULL)
				restart_daemon();

			FD_ZERO(&readfds);

			for (idx = 0; idx < ndaemons; idx++)
			{
				/* wait for a connection */
				if (Daemons[idx].d_socket >= 0)
				{
					if (!setproc &&
					    !bitnset(D_ETRNONLY,
						     Daemons[idx].d_flags))
					{
						sm_setproctitle(TRUE, e,
								"accepting connections");
						setproc = TRUE;
					}
					if (Daemons[idx].d_socket > highest)
						highest = Daemons[idx].d_socket;
					FD_SET((u_int)Daemons[idx].d_socket, &readfds);
				}
			}

# if NETUNIX
			if (ControlSocket >= 0)
			{
				if (ControlSocket > highest)
					highest = ControlSocket;
				FD_SET(ControlSocket, &readfds);
			}
# endif /* NETUNIX */

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			t = select(highest + 1, FDSET_CAST &readfds,
				   NULL, NULL, &timeout);

			/* Did someone signal while waiting? */
			if (ShutdownRequest != NULL)
				shutdown_daemon();
			else if (RestartRequest != NULL)
				restart_daemon();



			if (DoQueueRun)
				(void) runqueue(TRUE, FALSE);

			curdaemon = -1;
			if (t <= 0)
			{
				timedout = TRUE;
				break;
			}

			control = FALSE;
			errno = 0;

			/* look "round-robin" for an active socket */
			if ((idx = olddaemon + 1) >= ndaemons)
				idx = 0;
			for (i = 0; i < ndaemons; i++)
			{
				if (Daemons[idx].d_socket >= 0 &&
				    FD_ISSET(Daemons[idx].d_socket, &readfds))
				{
					lotherend = Daemons[idx].d_socksize;
					memset(&RealHostAddr, '\0',
					       sizeof RealHostAddr);
					t = accept(Daemons[idx].d_socket,
						   (struct sockaddr *)&RealHostAddr,
						   &lotherend);

					/*
					**  If remote side closes before
					**  accept() finishes, sockaddr
					**  might not be fully filled in.
					*/

					if (t >= 0 &&
					    (lotherend == 0 ||
# ifdef BSD4_4_SOCKADDR
					     RealHostAddr.sa.sa_len == 0 ||
# endif /* BSD4_4_SOCKADDR */
					     RealHostAddr.sa.sa_family != Daemons[idx].d_addr.sa.sa_family))
					{
						(void) close(t);
						t = -1;
						errno = EINVAL;
					}
					olddaemon = curdaemon = idx;
					break;
				}
				if (++idx >= ndaemons)
					idx = 0;
			}
# if NETUNIX
			if (curdaemon == -1 && ControlSocket >= 0 &&
			    FD_ISSET(ControlSocket, &readfds))
			{
				struct sockaddr_un sa_un;

				lotherend = sizeof sa_un;
				memset(&sa_un, '\0', sizeof sa_un);
				t = accept(ControlSocket,
					   (struct sockaddr *)&sa_un,
					   &lotherend);

				/*
				**  If remote side closes before
				**  accept() finishes, sockaddr
				**  might not be fully filled in.
				*/

				if (t >= 0 &&
				    (lotherend == 0 ||
# ifdef BSD4_4_SOCKADDR
				     sa_un.sun_len == 0 ||
# endif /* BSD4_4_SOCKADDR */
				     sa_un.sun_family != AF_UNIX))
				{
					(void) close(t);
					t = -1;
					errno = EINVAL;
				}
				if (t >= 0)
					control = TRUE;
			}
# else /* NETUNIX */
			if (curdaemon == -1)
			{
				/* No daemon to service */
				continue;
			}
# endif /* NETUNIX */
			if (t >= 0 || errno != EINTR)
				break;
		}
		if (timedout)
		{
			timedout = FALSE;
			continue;
		}
		save_errno = errno;
		timenow = curtime();
		(void) blocksignal(SIGALRM);
		if (t < 0)
		{
			errno = save_errno;
			syserr("getrequests: accept");

			/* arrange to re-open the socket next time around */
			(void) close(Daemons[curdaemon].d_socket);
			Daemons[curdaemon].d_socket = -1;
# if SO_REUSEADDR_IS_BROKEN
			/*
			**  Give time for bound socket to be released.
			**  This creates a denial-of-service if you can
			**  force accept() to fail on affected systems.
			*/

			Daemons[curdaemon].d_refuse_connections_until = timenow + 15;
# endif /* SO_REUSEADDR_IS_BROKEN */
			continue;
		}

		if (!control)
		{
			/* set some daemon related macros */
			switch (Daemons[curdaemon].d_addr.sa.sa_family)
			{
			  case AF_UNSPEC:
				define(macid("{daemon_family}", NULL),
				       "unspec", &BlankEnvelope);
				break;
# if NETINET
			  case AF_INET:
				define(macid("{daemon_family}", NULL),
				       "inet", &BlankEnvelope);
				break;
# endif /* NETINET */
# if NETINET6
			  case AF_INET6:
				define(macid("{daemon_family}", NULL),
				       "inet6", &BlankEnvelope);
				break;
# endif /* NETINET6 */
# if NETISO
			  case AF_ISO:
				define(macid("{daemon_family}", NULL),
				       "iso", &BlankEnvelope);
				break;
# endif /* NETISO */
# if NETNS
			  case AF_NS:
				define(macid("{daemon_family}", NULL),
				       "ns", &BlankEnvelope);
				break;
# endif /* NETNS */
# if NETX25
			  case AF_CCITT:
				define(macid("{daemon_family}", NULL),
				       "x.25", &BlankEnvelope);
				break;
# endif /* NETX25 */
			}
			define(macid("{daemon_name}", NULL),
			       Daemons[curdaemon].d_name, &BlankEnvelope);
			if (Daemons[curdaemon].d_mflags != NULL)
				define(macid("{daemon_flags}", NULL),
				       Daemons[curdaemon].d_mflags,
				       &BlankEnvelope);
			else
				define(macid("{daemon_flags}", NULL),
				       "", &BlankEnvelope);
		}

		/*
		**  Create a subprocess to process the mail.
		*/

		if (tTd(15, 2))
			dprintf("getrequests: forking (fd = %d)\n", t);

		/*
		**  advance state of PRNG
		**  this is necessary because otherwise all child processes
		**  will produce the same PRN sequence and hence the selection
		**  of a queue directory (and other things, e.g., MX selection)
		**  are not "really" random.
		*/
# if STARTTLS
		seed = get_random();
		RAND_seed((void *) &last_disk_space_check,
			sizeof last_disk_space_check);
		RAND_seed((void *) &timenow, sizeof timenow);
		RAND_seed((void *) &seed, sizeof seed);
# else /* STARTTLS */
		(void) get_random();
# endif /* STARTTLS */

#ifndef DEBUG_NO_FORK
		/*
		**  Create a pipe to keep the child from writing to the
		**  socket until after the parent has closed it.  Otherwise
		**  the parent may hang if the child has closed it first.
		*/

		if (pipe(pipefd) < 0)
			pipefd[0] = pipefd[1] = -1;

		(void) blocksignal(SIGCHLD);
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
			(void) sleep(10);
			(void) close(t);
			continue;
		}
#else /* ! DEBUG_NO_FORK */
		pid = 0;
#endif /* ! DEBUG_NO_FORK */

		if (pid == 0)
		{
			char *p;
			FILE *inchannel, *outchannel = NULL;

			/*
			**  CHILD -- return to caller.
			**	Collect verified idea of sending host.
			**	Verify calling user id if possible here.
			*/

			/* Reset global flags */
			RestartRequest = NULL;
			ShutdownRequest = NULL;
			PendingSignal = 0;

			(void) releasesignal(SIGALRM);
			(void) releasesignal(SIGCHLD);
			(void) setsignal(SIGCHLD, SIG_DFL);
			(void) setsignal(SIGHUP, SIG_DFL);
			(void) setsignal(SIGTERM, intsig);


			if (!control)
			{
				define(macid("{daemon_addr}", NULL),
				       newstr(anynet_ntoa(&Daemons[curdaemon].d_addr)),
				       &BlankEnvelope);
				(void) snprintf(status, sizeof status, "%d",
						ntohs(Daemons[curdaemon].d_port));
				define(macid("{daemon_port}", NULL),
				       newstr(status), &BlankEnvelope);
			}

			for (idx = 0; idx < ndaemons; idx++)
			{
				if (Daemons[idx].d_socket >= 0)
					(void) close(Daemons[idx].d_socket);
			}
			clrcontrol();

			/* Avoid SMTP daemon actions if control command */
			if (control)
			{
				/* Add control socket process */
				proc_list_add(getpid(), "console socket child",
					PROC_CONTROL_CHILD);
			}
			else
			{
				proc_list_clear();

				/* Add parent process as first child item */
				proc_list_add(getpid(), "daemon child",
					      PROC_DAEMON_CHILD);

				/* don't schedule queue runs if ETRN */
				QueueIntvl = 0;

				sm_setproctitle(TRUE, e, "startup with %s",
						anynet_ntoa(&RealHostAddr));
			}

#ifndef DEBUG_NO_FORK
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
#endif /* ! DEBUG_NO_FORK */

			/* control socket processing */
			if (control)
			{
				control_command(t, e);

				/* NOTREACHED */
				exit(EX_SOFTWARE);
			}

			/* determine host name */
			p = hostnamebyanyaddr(&RealHostAddr);
			if (strlen(p) > (SIZE_T) MAXNAME)
				p[MAXNAME] = '\0';
			RealHostName = newstr(p);
			if (RealHostName[0] == '[')
			{
				/* TEMP, FAIL: which one? */
				define(macid("{client_resolve}", NULL),
				       (h_errno == TRY_AGAIN) ? "TEMP" : "FAIL",
				       &BlankEnvelope);
			}
			else
				define(macid("{client_resolve}", NULL), "OK",
				       &BlankEnvelope);
			sm_setproctitle(TRUE, e, "startup with %s", p);

			if ((inchannel = fdopen(t, "r")) == NULL ||
			    (t = dup(t)) < 0 ||
			    (outchannel = fdopen(t, "w")) == NULL)
			{
				syserr("cannot open SMTP server channel, fd=%d", t);
				finis(FALSE, EX_OK);
			}

			InChannel = inchannel;
			OutChannel = outchannel;
			DisConnected = FALSE;

# ifdef XLA
			if (!xla_host_ok(RealHostName))
			{
				message("421 4.4.5 Too many SMTP sessions for this host");
				finis(FALSE, EX_OK);
			}
# endif /* XLA */
			/* find out name for interface of connection */
			if (getsockname(fileno(InChannel), &sa.sa,
					&len) == 0)
			{
				p = hostnamebyanyaddr(&sa);
				if (tTd(15, 9))
					dprintf("getreq: got name %s\n", p);
				define(macid("{if_name}", NULL),
				       newstr(p), &BlankEnvelope);

				/* do this only if it is not the loopback */
				/* interface: how to figure out? XXX */
				if (!isloopback(sa))
				{
					define(macid("{if_addr}", NULL),
					       newstr(anynet_ntoa(&sa)),
					       &BlankEnvelope);
					p = xalloc(5);
					snprintf(p, 4, "%d", sa.sa.sa_family);
					define(macid("{if_family}", NULL), p,
					       &BlankEnvelope);
					if (tTd(15, 7))
						dprintf("getreq: got addr %s and family %s\n",
							macvalue(macid("{if_addr}", NULL),
								 &BlankEnvelope),
							macvalue(macid("{if_addr}", NULL),
								 &BlankEnvelope));
				}
				else
				{
					define(macid("{if_addr}", NULL), NULL,
					       &BlankEnvelope);
					define(macid("{if_family}", NULL), NULL,
					       &BlankEnvelope);
				}
			}
			else
			{
				if (tTd(15, 7))
					dprintf("getreq: getsockname failed\n");
				define(macid("{if_name}", NULL), NULL,
				       &BlankEnvelope);
				define(macid("{if_addr}", NULL), NULL,
				       &BlankEnvelope);
				define(macid("{if_family}", NULL), NULL,
				       &BlankEnvelope);
			}
			break;
		}

		/* parent -- keep track of children */
		if (control)
		{
			snprintf(status, sizeof status, "control socket server child");
			proc_list_add(pid, status, PROC_CONTROL);
		}
		else
		{
			snprintf(status, sizeof status,
				 "SMTP server child for %s",
				 anynet_ntoa(&RealHostAddr));
			proc_list_add(pid, status, PROC_DAEMON);
		}
		(void) releasesignal(SIGCHLD);

		/* close the read end of the synchronization pipe */
		if (pipefd[0] != -1)
		{
			(void) close(pipefd[0]);
			pipefd[0] = -1;
		}

		/* close the port so that others will hang (for a while) */
		(void) close(t);

		/* release the child by closing the read end of the sync pipe */
		if (pipefd[1] != -1)
		{
			(void) close(pipefd[1]);
			pipefd[1] = -1;
		}
	}

	if (tTd(15, 2))
		dprintf("getreq: returning\n");
	return &Daemons[curdaemon].d_flags;
}
/*
**  OPENDAEMONSOCKET -- open SMTP socket
**
**	Deals with setting all appropriate options.
**
**	Parameters:
**		d -- the structure for the daemon to open.
**		firsttime -- set if this is the initial open.
**
**	Returns:
**		Size in bytes of the daemon socket addr.
**
**	Side Effects:
**		Leaves DaemonSocket set to the open socket.
**		Exits if the socket cannot be created.
*/

# define MAXOPENTRIES	10	/* maximum number of tries to open connection */

static int
opendaemonsocket(d, firsttime)
	struct daemon *d;
	bool firsttime;
{
	int on = 1;
	int fdflags;
	SOCKADDR_LEN_T socksize = 0;
	int ntries = 0;
	int save_errno;

	if (tTd(15, 2))
		dprintf("opendaemonsocket(%s)\n", d->d_name);

	do
	{
		if (ntries > 0)
			(void) sleep(5);
		if (firsttime || d->d_socket < 0)
		{
			d->d_socket = socket(d->d_addr.sa.sa_family,
					     SOCK_STREAM, 0);
			if (d->d_socket < 0)
			{
				save_errno = errno;
				syserr("opendaemonsocket: daemon %s: can't create server SMTP socket", d->d_name);
			  severe:
				if (LogLevel > 0)
					sm_syslog(LOG_ALERT, NOQID,
						  "daemon %s: problem creating SMTP socket", d->d_name);
				d->d_socket = -1;
				continue;
			}

			/* turn on network debugging? */
			if (tTd(15, 101))
				(void) setsockopt(d->d_socket, SOL_SOCKET,
						  SO_DEBUG, (char *)&on,
						  sizeof on);

			(void) setsockopt(d->d_socket, SOL_SOCKET,
					  SO_REUSEADDR, (char *)&on, sizeof on);
			(void) setsockopt(d->d_socket, SOL_SOCKET,
					  SO_KEEPALIVE, (char *)&on, sizeof on);

# ifdef SO_RCVBUF
			if (d->d_tcprcvbufsize > 0)
			{
				if (setsockopt(d->d_socket, SOL_SOCKET,
					       SO_RCVBUF,
					       (char *) &d->d_tcprcvbufsize,
					       sizeof(d->d_tcprcvbufsize)) < 0)
					syserr("opendaemonsocket: daemon %s: setsockopt(SO_RCVBUF)", d->d_name);
			}
# endif /* SO_RCVBUF */
# ifdef SO_SNDBUF
			if (d->d_tcpsndbufsize > 0)
			{
				if (setsockopt(d->d_socket, SOL_SOCKET,
					       SO_SNDBUF,
					       (char *) &d->d_tcpsndbufsize,
					       sizeof(d->d_tcpsndbufsize)) < 0)
					syserr("opendaemonsocket: daemon %s: setsockopt(SO_SNDBUF)", d->d_name);
			}
# endif /* SO_SNDBUF */

			if ((fdflags = fcntl(d->d_socket, F_GETFD, 0)) == -1 ||
			    fcntl(d->d_socket, F_SETFD,
				  fdflags | FD_CLOEXEC) == -1)
			{
				save_errno = errno;
				syserr("opendaemonsocket: daemon %s: failed to %s close-on-exec flag: %s",
				       d->d_name,
				       fdflags == -1 ? "get" : "set",
				       errstring(save_errno));
				(void) close(d->d_socket);
				goto severe;
			}

			switch (d->d_addr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				socksize = sizeof d->d_addr.sin;
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				socksize = sizeof d->d_addr.sin6;
				break;
# endif /* NETINET6 */

# if NETISO
			  case AF_ISO:
				socksize = sizeof d->d_addr.siso;
				break;
# endif /* NETISO */

			  default:
				socksize = sizeof d->d_addr;
				break;
			}

			if (bind(d->d_socket, &d->d_addr.sa, socksize) < 0)
			{
				/* probably another daemon already */
				save_errno = errno;
				syserr("opendaemonsocket: daemon %s: cannot bind",
				       d->d_name);
				(void) close(d->d_socket);
				goto severe;
			}
		}
		if (!firsttime &&
		    listen(d->d_socket, d->d_listenqueue) < 0)
		{
			save_errno = errno;
			syserr("opendaemonsocket: daemon %s: cannot listen",
			       d->d_name);
			(void) close(d->d_socket);
			goto severe;
		}
		return socksize;
	} while (ntries++ < MAXOPENTRIES && transienterror(save_errno));
	syserr("!opendaemonsocket: daemon %s: server SMTP socket wedged: exiting",
	       d->d_name);
	/* NOTREACHED */
	return -1;  /* avoid compiler warning on IRIX */
}
/*
**  SETUPDAEMON -- setup socket for daemon
**
**	Parameters:
**		daemonaddr -- socket for daemon
**		daemon -- number of daemon
**
**	Returns:
**		port number on which daemon should run
**
*/
static u_short
setupdaemon(daemonaddr)
	SOCKADDR *daemonaddr;
{
	u_short port;

	/*
	**  Set up the address for the mailer.
	*/

	if (daemonaddr->sa.sa_family == AF_UNSPEC)
	{
		memset(daemonaddr, '\0', sizeof *daemonaddr);
# if NETINET
		daemonaddr->sa.sa_family = AF_INET;
# endif /* NETINET */
	}

	switch (daemonaddr->sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		if (daemonaddr->sin.sin_addr.s_addr == 0)
			daemonaddr->sin.sin_addr.s_addr = INADDR_ANY;
		port = daemonaddr->sin.sin_port;
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&daemonaddr->sin6.sin6_addr))
			daemonaddr->sin6.sin6_addr = in6addr_any;
		port = daemonaddr->sin6.sin6_port;
		break;
# endif /* NETINET6 */

	  default:
		/* unknown protocol */
		port = 0;
		break;
	}
	if (port == 0)
	{
# ifdef NO_GETSERVBYNAME
		port = htons(25);
# else /* NO_GETSERVBYNAME */
		{
			register struct servent *sp;

			sp = getservbyname("smtp", "tcp");
			if (sp == NULL)
			{
				syserr("554 5.3.5 service \"smtp\" unknown");
				port = htons(25);
			}
			else
				port = sp->s_port;
		}
# endif /* NO_GETSERVBYNAME */
	}

	switch (daemonaddr->sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		daemonaddr->sin.sin_port = port;
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		daemonaddr->sin6.sin6_port = port;
		break;
# endif /* NETINET6 */

	  default:
		/* unknown protocol */
		break;
	}
	return(port);
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
	int i;

	for (i = 0; i < ndaemons; i++)
	{
		if (Daemons[i].d_socket >= 0)
			(void) close(Daemons[i].d_socket);
		Daemons[i].d_socket = -1;
	}
}
/*
**  SETSOCKADDROPTIONS -- set options for SOCKADDR (daemon or client)
**
**	Parameters:
**		p -- the options line.
**		d -- the daemon structure to fill in.
**
**	Returns:
**		none.
*/

static void
setsockaddroptions(p, d)
	register char *p;
	struct daemon *d;
{
# if NETISO
	short portno;
# endif /* NETISO */
	int l;
	char *h, *flags;
	char *port = NULL;
	char *addr = NULL;

# if NETINET
	if (d->d_addr.sa.sa_family == AF_UNSPEC)
		d->d_addr.sa.sa_family = AF_INET;
# endif /* NETINET */

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
				d->d_addr.sa.sa_family = atoi(v);
# if NETINET
			else if (strcasecmp(v, "inet") == 0)
				d->d_addr.sa.sa_family = AF_INET;
# endif /* NETINET */
# if NETINET6
			else if (strcasecmp(v, "inet6") == 0)
				d->d_addr.sa.sa_family = AF_INET6;
# endif /* NETINET6 */
# if NETISO
			else if (strcasecmp(v, "iso") == 0)
				d->d_addr.sa.sa_family = AF_ISO;
# endif /* NETISO */
# if NETNS
			else if (strcasecmp(v, "ns") == 0)
				d->d_addr.sa.sa_family = AF_NS;
# endif /* NETNS */
# if NETX25
			else if (strcasecmp(v, "x.25") == 0)
				d->d_addr.sa.sa_family = AF_CCITT;
# endif /* NETX25 */
			else
				syserr("554 5.3.5 Unknown address family %s in Family=option",
				       v);
			break;

		  case 'A':		/* address */
			addr = v;
			break;

		  case 'P':		/* port */
			port = v;
			break;

		  case 'L':		/* listen queue size */
			d->d_listenqueue = atoi(v);
			break;

		  case 'M':		/* modifiers (flags) */
			l = 3 * strlen(v) + 3;
			h = v;
			flags = xalloc(l);
			d->d_mflags = flags;
			for (; *h != '\0'; h++)
			{
				if (!(isascii(*h) && isspace(*h)))
				{
					if (flags != d->d_mflags)
						*flags++ = ' ';
					*flags++ = *h;
					if (isupper(*h))
						*flags++ = *h;
				}
			}
			*flags++ = '\0';
			for (; *v != '\0'; v++)
				if (!(isascii(*v) && isspace(*v)))
					setbitn(bitidx(*v), d->d_flags);
			break;

		  case 'S':		/* send buffer size */
			d->d_tcpsndbufsize = atoi(v);
			break;

		  case 'R':		/* receive buffer size */
			d->d_tcprcvbufsize = atoi(v);
			break;

		  case 'N':		/* name */
			d->d_name = v;
			break;

		  default:
			syserr("554 5.3.5 PortOptions parameter \"%s\" unknown",
			       f);
		}
	}

	/* Check addr and port after finding family */
	if (addr != NULL)
	{
		switch (d->d_addr.sa.sa_family)
		{
# if NETINET
		  case AF_INET:
			if (!isascii(*addr) || !isdigit(*addr) ||
			    ((d->d_addr.sin.sin_addr.s_addr = inet_addr(addr)) == INADDR_NONE))
			{
				register struct hostent *hp;

				hp = sm_gethostbyname(addr, AF_INET);
				if (hp == NULL)
					syserr("554 5.3.0 host \"%s\" unknown",
					       addr);
				else
				{
					while (*(hp->h_addr_list) != NULL &&
					       hp->h_addrtype != AF_INET)
						hp->h_addr_list++;
					if (*(hp->h_addr_list) == NULL)
						syserr("554 5.3.0 host \"%s\" unknown",
						       addr);
					else
						memmove(&d->d_addr.sin.sin_addr,
							*(hp->h_addr_list),
							INADDRSZ);
#  if _FFR_FREEHOSTENT && NETINET6
					freehostent(hp);
					hp = NULL;
#  endif /* _FFR_FREEHOSTENT && NETINET6 */
				}
			}
			break;
# endif /* NETINET */

# if NETINET6
		  case AF_INET6:
			if (!isascii(*addr) ||
			    (!isxdigit(*addr) && *addr != ':') ||
			    inet_pton(AF_INET6, addr,
				      &d->d_addr.sin6.sin6_addr) != 1)
			{
				register struct hostent *hp;

				hp = sm_gethostbyname(addr, AF_INET6);
				if (hp == NULL)
					syserr("554 5.3.0 host \"%s\" unknown",
					       addr);
				else
				{
					while (*(hp->h_addr_list) != NULL &&
					       hp->h_addrtype != AF_INET6)
						hp->h_addr_list++;
					if (*(hp->h_addr_list) == NULL)
						syserr("554 5.3.0 host \"%s\" unknown",
						       addr);
					else
						memmove(&d->d_addr.sin6.sin6_addr,
							*(hp->h_addr_list),
							IN6ADDRSZ);
#  if _FFR_FREEHOSTENT
					freehostent(hp);
					hp = NULL;
#  endif /* _FFR_FREEHOSTENT */
				}
			}
			break;
# endif /* NETINET6 */

		  default:
			syserr("554 5.3.5 address= option unsupported for family %d",
			       d->d_addr.sa.sa_family);
			break;
		}
	}

	if (port != NULL)
	{
		switch (d->d_addr.sa.sa_family)
		{
# if NETINET
		  case AF_INET:
			if (isascii(*port) && isdigit(*port))
				d->d_addr.sin.sin_port = htons((u_short)atoi((const char *)port));
			else
			{
#  ifdef NO_GETSERVBYNAME
				syserr("554 5.3.5 invalid port number: %s",
				       port);
#  else /* NO_GETSERVBYNAME */
				register struct servent *sp;

				sp = getservbyname(port, "tcp");
				if (sp == NULL)
					syserr("554 5.3.5 service \"%s\" unknown",
					       port);
				else
					d->d_addr.sin.sin_port = sp->s_port;
#  endif /* NO_GETSERVBYNAME */
			}
			break;
# endif /* NETINET */

# if NETINET6
		  case AF_INET6:
			if (isascii(*port) && isdigit(*port))
				d->d_addr.sin6.sin6_port = htons((u_short)atoi(port));
			else
			{
#  ifdef NO_GETSERVBYNAME
				syserr("554 5.3.5 invalid port number: %s",
				       port);
#  else /* NO_GETSERVBYNAME */
				register struct servent *sp;

				sp = getservbyname(port, "tcp");
				if (sp == NULL)
					syserr("554 5.3.5 service \"%s\" unknown",
					       port);
				else
					d->d_addr.sin6.sin6_port = sp->s_port;
#  endif /* NO_GETSERVBYNAME */
			}
			break;
# endif /* NETINET6 */

# if NETISO
		  case AF_ISO:
			/* assume two byte transport selector */
			if (isascii(*port) && isdigit(*port))
				portno = htons((u_short)atoi(port));
			else
			{
#  ifdef NO_GETSERVBYNAME
				syserr("554 5.3.5 invalid port number: %s",
				       port);
#  else /* NO_GETSERVBYNAME */
				register struct servent *sp;

				sp = getservbyname(port, "tcp");
				if (sp == NULL)
					syserr("554 5.3.5 service \"%s\" unknown",
					       port);
				else
					portno = sp->s_port;
#  endif /* NO_GETSERVBYNAME */
			}
			memmove(TSEL(&d->d_addr.siso),
				(char *) &portno, 2);
			break;
# endif /* NETISO */

		  default:
			syserr("554 5.3.5 Port= option unsupported for family %d",
			       d->d_addr.sa.sa_family);
			break;
		}
	}
}
/*
**  SETDAEMONOPTIONS -- set options for running the MTA daemon
**
**	Parameters:
**		p -- the options line.
**
**	Returns:
**		TRUE if successful, FALSE otherwise.
*/

bool
setdaemonoptions(p)
	register char *p;
{
	if (ndaemons >= MAXDAEMONS)
		return FALSE;
	Daemons[ndaemons].d_socket = -1;
	Daemons[ndaemons].d_listenqueue = 10;
	clrbitmap(Daemons[ndaemons].d_flags);
	setsockaddroptions(p, &Daemons[ndaemons]);

	if (Daemons[ndaemons].d_name != NULL)
		Daemons[ndaemons].d_name = newstr(Daemons[ndaemons].d_name);
	else
	{
		char num[30];

		snprintf(num, sizeof num, "Daemon%d", ndaemons);
		Daemons[ndaemons].d_name = newstr(num);
	}

	if (tTd(37, 1))
	{
		dprintf("Daemon %s flags: ", Daemons[ndaemons].d_name);
		if (bitnset(D_ETRNONLY, Daemons[ndaemons].d_flags))
			dprintf("ETRNONLY ");
		if (bitnset(D_NOETRN, Daemons[ndaemons].d_flags))
			dprintf("NOETRN ");
		dprintf("\n");
	}
	++ndaemons;
	return TRUE;
}
/*
**  INITDAEMON -- initialize daemon if not yet done.
**
**	Parameters:
**		none
**
**	Returns:
**		none
**
**	Side Effects:
**		initializes structure for one daemon.
*/
void
initdaemon()
{
	if (ndaemons == 0)
	{
		Daemons[ndaemons].d_socket = -1;
		Daemons[ndaemons].d_listenqueue = 10;
		Daemons[ndaemons].d_name = "Daemon0";
		ndaemons = 1;
	}
}
/*
**  SETCLIENTOPTIONS -- set options for running the client
**
**	Parameters:
**		p -- the options line.
**
**	Returns:
**		none.
*/

static SOCKADDR	ClientAddr;		/* address for client */

void
setclientoptions(p)
	register char *p;
{
	struct daemon d;
	extern ENVELOPE BlankEnvelope;

	memset(&d, '\0', sizeof d);
	setsockaddroptions(p, &d);

	/* grab what we need */
	memcpy(&ClientAddr, &d.d_addr, sizeof ClientAddr);
	TcpSndBufferSize = d.d_tcpsndbufsize;
	TcpRcvBufferSize = d.d_tcprcvbufsize;
	if (d.d_mflags != NULL)
		define(macid("{client_flags}", NULL), d.d_mflags,
		       &BlankEnvelope);
	else
		define(macid("{client_flags}", NULL), "", &BlankEnvelope);
}
/*
**  ADDR_FAMILY -- determine address family from address
**
**	Parameters:
**		addr -- the string representation of the address
**
**	Returns:
**		AF_INET, AF_INET6 or AF_UNSPEC
**
**	Side Effects:
**		none.
*/

static int
addr_family(addr)
	char *addr;
{
# if NETINET6
	SOCKADDR clt_addr;
# endif /* NETINET6 */

# if NETINET
	if (inet_addr(addr) != INADDR_NONE)
	{
		if (tTd(16, 9))
			printf("addr_family(%s): INET\n", addr);
		return AF_INET;
	}
# endif /* NETINET */
# if NETINET6
	if (inet_pton(AF_INET6, addr, &clt_addr.sin6.sin6_addr) == 1)
	{
		if (tTd(16, 9))
			printf("addr_family(%s): INET6\n", addr);
		return AF_INET6;
	}
# endif /* NETINET6 */
	if (tTd(16, 9))
		printf("addr_family(%s): UNSPEC\n", addr);
	return AF_UNSPEC;
}
/*
**  MAKECONNECTION -- make a connection to an SMTP socket on a machine.
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

SOCKADDR	CurHostAddr;		/* address of current host */

int
makeconnection(host, port, mci, e)
	char *host;
	volatile u_int port;
	register MCI *mci;
	ENVELOPE *e;
{
	register volatile int addrno = 0;
	register volatile int s;
	register struct hostent *volatile hp = (struct hostent *)NULL;
	SOCKADDR addr;
	SOCKADDR clt_addr;
	int save_errno = 0;
	volatile SOCKADDR_LEN_T addrlen;
	volatile bool firstconnect;
	EVENT *volatile ev = NULL;
# if NETINET6
	volatile bool v6found = FALSE;
# endif /* NETINET6 */
	volatile int family = InetMode;
	SOCKADDR_LEN_T len;
	volatile SOCKADDR_LEN_T socksize = 0;
	volatile bool clt_bind;
	BITMAP256 d_flags;
	char *p;
	extern ENVELOPE BlankEnvelope;

	/* retranslate ${daemon_flags} into bitmap */
	clrbitmap(d_flags);
	if ((p = macvalue(macid("{daemon_flags}", NULL), e)) != NULL)
	{
		for (; *p != '\0'; p++)
		{
			if (!(isascii(*p) && isspace(*p)))
				setbitn(bitidx(*p), d_flags);
		}
	}

	/* "add" ${client_flags} to bitmap */
	if ((p = macvalue(macid("{client_flags}", NULL), e)) != NULL)
	{
		for (; *p != '\0'; p++)
		{
			/* look for just this one flag */
			if (*p == D_IFNHELO)
			{
				setbitn(bitidx(*p), d_flags);
				break;
			}
		}
	}

# if NETINET6
 v4retry:
# endif /* NETINET6 */
	clt_bind = FALSE;

	/* Set up the address for outgoing connection. */
	if (bitnset(D_BINDIF, d_flags) &&
	    (p = macvalue(macid("{if_addr}", NULL), e)) != NULL &&
	    *p != '\0')
	{
# if NETINET6
		char p6[INET6_ADDRSTRLEN];
# endif /* NETINET6 */

		memset(&clt_addr, '\0', sizeof clt_addr);

		/* infer the address family from the address itself */
		clt_addr.sa.sa_family = addr_family(p);
		switch (clt_addr.sa.sa_family)
		{
# if NETINET
		  case AF_INET:
			clt_addr.sin.sin_addr.s_addr = inet_addr(p);
			if (clt_addr.sin.sin_addr.s_addr != INADDR_NONE &&
			    clt_addr.sin.sin_addr.s_addr != INADDR_LOOPBACK)
			{
				clt_bind = TRUE;
				socksize = sizeof (struct sockaddr_in);
			}
			break;
# endif /* NETINET */

# if NETINET6
		  case AF_INET6:
			if (inet_addr(p) != INADDR_NONE)
				snprintf(p6, sizeof p6, "::ffff:%s", p);
			else
				strlcpy(p6, p, sizeof p6);
			if (inet_pton(AF_INET6, p6,
				      &clt_addr.sin6.sin6_addr) == 1 &&
			    !IN6_IS_ADDR_LOOPBACK(&clt_addr.sin6.sin6_addr))
			{
				clt_bind = TRUE;
				socksize = sizeof (struct sockaddr_in6);
			}
			break;
# endif /* NETINET6 */

# if 0
		  default:
			syserr("554 5.3.5 Address= option unsupported for family %d",
			       clt_addr.sa.sa_family);
			break;
# endif /* 0 */
		}
		if (clt_bind)
			family = clt_addr.sa.sa_family;
	}
	else
	{
		STRUCTCOPY(ClientAddr, clt_addr);
		if (clt_addr.sa.sa_family == AF_UNSPEC)
			clt_addr.sa.sa_family = family;
		switch (clt_addr.sa.sa_family)
		{
# if NETINET
		  case AF_INET:
			if (clt_addr.sin.sin_addr.s_addr == 0)
				clt_addr.sin.sin_addr.s_addr = INADDR_ANY;
			else
				clt_bind = TRUE;
			if (clt_addr.sin.sin_port != 0)
				clt_bind = TRUE;
			socksize = sizeof (struct sockaddr_in);
			break;
# endif /* NETINET */
# if NETINET6
		  case AF_INET6:
			if (IN6_IS_ADDR_UNSPECIFIED(&clt_addr.sin6.sin6_addr))
				clt_addr.sin6.sin6_addr = in6addr_any;
			else
				clt_bind = TRUE;
			socksize = sizeof (struct sockaddr_in6);
			if (clt_addr.sin6.sin6_port != 0)
				clt_bind = TRUE;
			break;
# endif /* NETINET6 */
# if NETISO
		  case AF_ISO:
			socksize = sizeof clt_addr.siso;
			clt_bind = TRUE;
			break;
# endif /* NETISO */
		  default:
			break;
		}
	}

	/*
	**  Set up the address for the mailer.
	**	Accept "[a.b.c.d]" syntax for host name.
	*/

# if NAMED_BIND
	SM_SET_H_ERRNO(0);
# endif /* NAMED_BIND */
	errno = 0;
	memset(&CurHostAddr, '\0', sizeof CurHostAddr);
	memset(&addr, '\0', sizeof addr);
	SmtpPhase = mci->mci_phase = "initial connection";
	CurHostName = host;

	if (host[0] == '[')
	{
		p = strchr(host, ']');
		if (p != NULL)
		{
# if NETINET
			unsigned long hid = INADDR_NONE;
# endif /* NETINET */
# if NETINET6
			struct sockaddr_in6 hid6;
# endif /* NETINET6 */

			*p = '\0';
# if NETINET6
			memset(&hid6, '\0', sizeof hid6);
# endif /* NETINET6 */
# if NETINET
			if (family == AF_INET &&
			    (hid = inet_addr(&host[1])) != INADDR_NONE)
			{
				addr.sin.sin_family = AF_INET;
				addr.sin.sin_addr.s_addr = hid;
			}
			else
# endif /* NETINET */
# if NETINET6
			if (family == AF_INET6 &&
			    inet_pton(AF_INET6, &host[1],
				      &hid6.sin6_addr) == 1)
			{
				addr.sin6.sin6_family = AF_INET6;
				addr.sin6.sin6_addr = hid6.sin6_addr;
			}
			else
# endif /* NETINET6 */
			{
				/* try it as a host name (avoid MX lookup) */
				hp = sm_gethostbyname(&host[1], family);
				if (hp == NULL && p[-1] == '.')
				{
# if NAMED_BIND
					int oldopts = _res.options;

					_res.options &= ~(RES_DEFNAMES|RES_DNSRCH);
# endif /* NAMED_BIND */
					p[-1] = '\0';
					hp = sm_gethostbyname(&host[1],
							      family);
					p[-1] = '.';
# if NAMED_BIND
					_res.options = oldopts;
# endif /* NAMED_BIND */
				}
				*p = ']';
				goto gothostent;
			}
			*p = ']';
		}
		if (p == NULL)
		{
			extern char MsgBuf[];

			usrerrenh("5.1.2",
				  "553 Invalid numeric domain spec \"%s\"",
				  host);
			mci_setstat(mci, EX_NOHOST, "5.1.2", MsgBuf);
			errno = EINVAL;
			return EX_NOHOST;
		}
	}
	else
	{
		/* contortion to get around SGI cc complaints */
		{
			p = &host[strlen(host) - 1];
			hp = sm_gethostbyname(host, family);
			if (hp == NULL && *p == '.')
			{
# if NAMED_BIND
				int oldopts = _res.options;

				_res.options &= ~(RES_DEFNAMES|RES_DNSRCH);
# endif /* NAMED_BIND */
				*p = '\0';
				hp = sm_gethostbyname(host, family);
				*p = '.';
# if NAMED_BIND
				_res.options = oldopts;
# endif /* NAMED_BIND */
			}
		}
gothostent:
		if (hp == NULL)
		{
# if NAMED_BIND
			/* check for name server timeouts */
			if (errno == ETIMEDOUT || h_errno == TRY_AGAIN ||
			    (errno == ECONNREFUSED && UseNameServer))
			{
				save_errno = errno;
				mci_setstat(mci, EX_TEMPFAIL, "4.4.3", NULL);
				errno = save_errno;
				return EX_TEMPFAIL;
			}
# endif /* NAMED_BIND */
# if NETINET6
			/*
			**  Try v6 first, then fall back to v4.
			**  If we found a v6 address, but no v4
			**  addresses, then TEMPFAIL.
			*/

			if (family == AF_INET6)
			{
				family = AF_INET;
				goto v4retry;
			}
			if (v6found)
				goto v6tempfail;
# endif /* NETINET6 */
			save_errno = errno;
			mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
			errno = save_errno;
			return EX_NOHOST;
		}
		addr.sa.sa_family = hp->h_addrtype;
		switch (hp->h_addrtype)
		{
# if NETINET
		  case AF_INET:
			memmove(&addr.sin.sin_addr,
				hp->h_addr,
				INADDRSZ);
			break;
# endif /* NETINET */

# if NETINET6
		  case AF_INET6:
			memmove(&addr.sin6.sin6_addr,
				hp->h_addr,
				IN6ADDRSZ);
			break;
# endif /* NETINET6 */

		  default:
			if (hp->h_length > sizeof addr.sa.sa_data)
			{
				syserr("makeconnection: long sa_data: family %d len %d",
					hp->h_addrtype, hp->h_length);
				mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
				errno = EINVAL;
				return EX_NOHOST;
			}
			memmove(addr.sa.sa_data,
				hp->h_addr,
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
# ifdef NO_GETSERVBYNAME
		port = htons(25);
# else /* NO_GETSERVBYNAME */
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
# endif /* NO_GETSERVBYNAME */
	}

	switch (addr.sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		addr.sin.sin_port = port;
		addrlen = sizeof (struct sockaddr_in);
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		addr.sin6.sin6_port = port;
		addrlen = sizeof (struct sockaddr_in6);
		break;
# endif /* NETINET6 */

# if NETISO
	  case AF_ISO:
		/* assume two byte transport selector */
		memmove(TSEL((struct sockaddr_iso *) &addr), (char *) &port, 2);
		addrlen = sizeof (struct sockaddr_iso);
		break;
# endif /* NETISO */

	  default:
		syserr("Can't connect to address family %d", addr.sa.sa_family);
		mci_setstat(mci, EX_NOHOST, "5.1.2", NULL);
		errno = EINVAL;
# if _FFR_FREEHOSTENT && NETINET6
		if (hp != NULL)
			freehostent(hp);
# endif /* _FFR_FREEHOSTENT && NETINET6 */
		return EX_NOHOST;
	}

	/*
	**  Try to actually open the connection.
	*/

# ifdef XLA
	/* if too many connections, don't bother trying */
	if (!xla_noqueue_ok(host))
	{
#  if _FFR_FREEHOSTENT && NETINET6
		if (hp != NULL)
			freehostent(hp);
#  endif /* _FFR_FREEHOSTENT && NETINET6 */
		return EX_TEMPFAIL;
	}
# endif /* XLA */

	firstconnect = TRUE;
	for (;;)
	{
		if (tTd(16, 1))
			dprintf("makeconnection (%s [%s].%d (%d))\n",
				host, anynet_ntoa(&addr), ntohs(port),
				addr.sa.sa_family);

		/* save for logging */
		CurHostAddr = addr;

		if (bitnset(M_SECURE_PORT, mci->mci_mailer->m_flags))
		{
			int rport = IPPORT_RESERVED - 1;

			s = rresvport(&rport);
		}
		else
		{
			s = socket(clt_addr.sa.sa_family, SOCK_STREAM, 0);
		}
		if (s < 0)
		{
			save_errno = errno;
			syserr("makeconnection: cannot create socket");
# ifdef XLA
			xla_host_end(host);
# endif /* XLA */
			mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
# if _FFR_FREEHOSTENT && NETINET6
			if (hp != NULL)
				freehostent(hp);
# endif /* _FFR_FREEHOSTENT && NETINET6 */
			errno = save_errno;
			return EX_TEMPFAIL;
		}

# ifdef SO_SNDBUF
		if (TcpSndBufferSize > 0)
		{
			if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
				       (char *) &TcpSndBufferSize,
				       sizeof(TcpSndBufferSize)) < 0)
				syserr("makeconnection: setsockopt(SO_SNDBUF)");
		}
# endif /* SO_SNDBUF */
# ifdef SO_RCVBUF
		if (TcpRcvBufferSize > 0)
		{
			if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
				       (char *) &TcpRcvBufferSize,
				       sizeof(TcpRcvBufferSize)) < 0)
				syserr("makeconnection: setsockopt(SO_RCVBUF)");
		}
# endif /* SO_RCVBUF */


		if (tTd(16, 1))
			dprintf("makeconnection: fd=%d\n", s);

		/* turn on network debugging? */
		if (tTd(16, 101))
		{
			int on = 1;

			(void) setsockopt(s, SOL_SOCKET, SO_DEBUG,
					  (char *)&on, sizeof on);
		}
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);	/* for debugging */
		errno = 0;				/* for debugging */

		if (clt_bind)
		{
			int on = 1;

			switch (clt_addr.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				if (clt_addr.sin.sin_port != 0)
					(void) setsockopt(s, SOL_SOCKET,
							  SO_REUSEADDR,
							  (char *) &on,
							  sizeof on);
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				if (clt_addr.sin6.sin6_port != 0)
					(void) setsockopt(s, SOL_SOCKET,
							  SO_REUSEADDR,
							  (char *) &on,
							  sizeof on);
				break;
# endif /* NETINET6 */
			}

			if (bind(s, &clt_addr.sa, socksize) < 0)
			{
				save_errno = errno;
				(void) close(s);
				errno = save_errno;
				syserr("makeconnection: cannot bind socket [%s]",
				       anynet_ntoa(&clt_addr));
# if _FFR_FREEHOSTENT && NETINET6
				if (hp != NULL)
					freehostent(hp);
# endif /* _FFR_FREEHOSTENT && NETINET6 */
				errno = save_errno;
				return EX_TEMPFAIL;
			}
		}

		/*
		**  Linux seems to hang in connect for 90 minutes (!!!).
		**  Time out the connect to avoid this problem.
		*/

		if (setjmp(CtxConnectTimeout) == 0)
		{
			int i;

			if (e->e_ntries <= 0 && TimeOuts.to_iconnect != 0)
				ev = setevent(TimeOuts.to_iconnect,
					      connecttimeout, 0);
			else if (TimeOuts.to_connect != 0)
				ev = setevent(TimeOuts.to_connect,
					      connecttimeout, 0);
			else
				ev = NULL;

			switch (ConnectOnlyTo.sa.sa_family)
			{
# if NETINET
			  case AF_INET:
				addr.sin.sin_addr.s_addr = ConnectOnlyTo.sin.sin_addr.s_addr;
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				memmove(&addr.sin6.sin6_addr,
					&ConnectOnlyTo.sin6.sin6_addr,
					IN6ADDRSZ);
				break;
# endif /* NETINET6 */
			}
			i = connect(s, (struct sockaddr *) &addr, addrlen);
			save_errno = errno;
			if (ev != NULL)
				clrevent(ev);
			if (i >= 0)
				break;
		}
		else
			save_errno = errno;

		/* if running demand-dialed connection, try again */
		if (DialDelay > 0 && firstconnect)
		{
			if (tTd(16, 1))
				dprintf("Connect failed (%s); trying again...\n",
					errstring(save_errno));
			firstconnect = FALSE;
			(void) sleep(DialDelay);
			continue;
		}

		/* couldn't connect.... figure out why */
		(void) close(s);

		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, e->e_id,
				  "makeconnection (%s [%s]) failed: %s",
				  host, anynet_ntoa(&addr),
				  errstring(save_errno));

		if (hp != NULL && hp->h_addr_list[addrno] != NULL)
		{
			if (tTd(16, 1))
				dprintf("Connect failed (%s); trying new address....\n",
					errstring(save_errno));
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
				memmove(addr.sa.sa_data,
					hp->h_addr_list[addrno++],
					hp->h_length);
				break;
			}
			continue;
		}
		errno = save_errno;

# if NETINET6
		if (family == AF_INET6)
		{
			if (tTd(16, 1))
				dprintf("Connect failed (%s); retrying with AF_INET....\n",
					errstring(save_errno));
			v6found = TRUE;
			family = AF_INET;
#  if _FFR_FREEHOSTENT
			if (hp != NULL)
			{
				freehostent(hp);
				hp = NULL;
			}
#  endif /* _FFR_FREEHOSTENT */
			goto v4retry;
		}
	v6tempfail:
# endif /* NETINET6 */
		/* couldn't open connection */
# if NETINET6
		/* Don't clobber an already saved errno from v4retry */
		if (errno > 0)
# endif /* NETINET6 */
			save_errno = errno;
		if (tTd(16, 1))
			dprintf("Connect failed (%s)\n", errstring(save_errno));
# ifdef XLA
		xla_host_end(host);
# endif /* XLA */
		mci_setstat(mci, EX_TEMPFAIL, "4.4.1", NULL);
# if _FFR_FREEHOSTENT && NETINET6
		if (hp != NULL)
			freehostent(hp);
# endif /* _FFR_FREEHOSTENT && NETINET6 */
		errno = save_errno;
		return EX_TEMPFAIL;
	}

# if _FFR_FREEHOSTENT && NETINET6
	if (hp != NULL)
	{
		freehostent(hp);
		hp = NULL;
	}
# endif /* _FFR_FREEHOSTENT && NETINET6 */

	/* connection ok, put it into canonical form */
	mci->mci_out = NULL;
	if ((mci->mci_out = fdopen(s, "w")) == NULL ||
	    (s = dup(s)) < 0 ||
	    (mci->mci_in = fdopen(s, "r")) == NULL)
	{
		save_errno = errno;
		syserr("cannot open SMTP client channel, fd=%d", s);
		mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
		if (mci->mci_out != NULL)
			(void) fclose(mci->mci_out);
		(void) close(s);
		errno = save_errno;
		return EX_TEMPFAIL;
	}

	/* find out name for Interface through which we connect */
	len = sizeof addr;
	if (getsockname(s, &addr.sa, &len) == 0)
	{
		char *name;
		char *p;

		define(macid("{if_addr}", NULL), newstr(anynet_ntoa(&addr)),
		       &BlankEnvelope);
		p = xalloc(5);
		snprintf(p, 4, "%d", addr.sa.sa_family);
		define(macid("{if_family}", NULL), p, &BlankEnvelope);

		name = hostnamebyanyaddr(&addr);
		define(macid("{if_name}", NULL), newstr(name), &BlankEnvelope);
		if (LogLevel > 11)
		{
			/* log connection information */
			sm_syslog(LOG_INFO, e->e_id,
				  "SMTP outgoing connect on %.40s", name);
		}
		if (bitnset(D_IFNHELO, d_flags))
		{
			if (name[0] != '[' && strchr(name, '.') != NULL)
				mci->mci_heloname = newstr(name);
		}
	}
	else
	{
		define(macid("{if_name}", NULL), NULL, &BlankEnvelope);
		define(macid("{if_addr}", NULL), NULL, &BlankEnvelope);
		define(macid("{if_family}", NULL), NULL, &BlankEnvelope);
	}
	mci_setstat(mci, EX_OK, NULL, NULL);
	return EX_OK;
}

static void
connecttimeout()
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(CtxConnectTimeout, 1);
}
/*
**  MAKECONNECTION_DS -- make a connection to a domain socket.
**
**	Parameters:
**		mux_path -- the path of the socket to connect to.
**		mci -- a pointer to the mail connection information
**			structure to be filled in.
**
**	Returns:
**		An exit code telling whether the connection could be
**			made and if not why not.
**
**	Side Effects:
**		none.
*/

# if NETUNIX
int makeconnection_ds(mux_path, mci)
	char *mux_path;
	register MCI *mci;
{
	int sock;
	int rval, save_errno;
	long sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_ROOTOK|SFF_EXECOK;
	struct sockaddr_un unix_addr;

	/* if not safe, don't connect */
	rval = safefile(mux_path, RunAsUid, RunAsGid, RunAsUserName,
			sff, S_IRUSR|S_IWUSR, NULL);

	if (rval != 0)
	{
		syserr("makeconnection_ds: unsafe domain socket");
		mci_setstat(mci, EX_TEMPFAIL, "4.3.5", NULL);
		errno = rval;
		return EX_TEMPFAIL;
	}

	/* prepare address structure */
	memset(&unix_addr, '\0', sizeof unix_addr);
	unix_addr.sun_family = AF_UNIX;

	if (strlen(mux_path) >= sizeof unix_addr.sun_path)
	{
		syserr("makeconnection_ds: domain socket name too long");
		/* XXX why TEMPFAIL ? */
		mci_setstat(mci, EX_TEMPFAIL, "5.3.5", NULL);
		errno = ENAMETOOLONG;
		return EX_UNAVAILABLE;
	}
	(void) strlcpy(unix_addr.sun_path, mux_path, sizeof unix_addr.sun_path);

	/* initialize domain socket */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
	{
		save_errno = errno;
		syserr("makeconnection_ds: could not create domain socket");
		mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
		errno = save_errno;
		return EX_TEMPFAIL;
	}

	/* connect to server */
	if (connect(sock, (struct sockaddr *) &unix_addr,
		    sizeof(unix_addr)) == -1)
	{
		save_errno = errno;
		syserr("Could not connect to socket %s", mux_path);
		mci_setstat(mci, EX_TEMPFAIL, "4.4.1", NULL);
		(void) close(sock);
		errno = save_errno;
		return EX_TEMPFAIL;
	}

	/* connection ok, put it into canonical form */
	mci->mci_out = NULL;
	if ((mci->mci_out = fdopen(sock, "w")) == NULL ||
	    (sock = dup(sock)) < 0 ||
	    (mci->mci_in = fdopen(sock, "r")) == NULL)
	{
		save_errno = errno;
		syserr("cannot open SMTP client channel, fd=%d", sock);
		mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
		if (mci->mci_out != NULL)
			(void) fclose(mci->mci_out);
		(void) close(sock);
		errno = save_errno;
		return EX_TEMPFAIL;
	}

	mci_setstat(mci, EX_OK, NULL, NULL);
	errno = 0;
	return EX_OK;
}
# endif /* NETUNIX */
/*
**  SIGHUP -- handle a SIGHUP signal
**
**	Parameters:
**		sig -- incoming signal.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets RestartRequest which should cause the daemon
**		to restart.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

/* ARGSUSED */
static SIGFUNC_DECL
sighup(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, sighup);
	RestartRequest = "signal";
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  RESTART_DAEMON -- Performs a clean restart of the daemon
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		restarts the daemon or exits if restart fails.
*/

static void
restart_daemon()
{
	int i;
	int save_errno;
	char *reason;
	sigfunc_t oalrm, ochld, ohup, oint, opipe, oterm, ousr1;
	extern int DtableSize;

	allsignals(TRUE);

	reason = RestartRequest;
	RestartRequest = NULL;
	PendingSignal = 0;

	if (SaveArgv[0][0] != '/')
	{
		if (LogLevel > 3)
			sm_syslog(LOG_INFO, NOQID,
				  "could not restart: need full path");
		finis(FALSE, EX_OSFILE);
	}
	if (LogLevel > 3)
		sm_syslog(LOG_INFO, NOQID, "restarting %s due to %s",
			  SaveArgv[0],
			  reason == NULL ? "implicit call" : reason);

	closecontrolsocket(TRUE);
	if (drop_privileges(TRUE) != EX_OK)
	{
		if (LogLevel > 0)
			sm_syslog(LOG_ALERT, NOQID,
				  "could not set[ug]id(%d, %d): %m",
				  RunAsUid, RunAsGid);
		finis(FALSE, EX_OSERR);
	}

	/* arrange for all the files to be closed */
	for (i = 3; i < DtableSize; i++)
	{
		register int j;

		if ((j = fcntl(i, F_GETFD, 0)) != -1)
			(void) fcntl(i, F_SETFD, j | FD_CLOEXEC);
	}

	/* need to allow signals before execve() so make them harmless */
	oalrm = setsignal(SIGALRM, SIG_DFL);
	ochld = setsignal(SIGCHLD, SIG_DFL);
	ohup = setsignal(SIGHUP, SIG_DFL);
	oint = setsignal(SIGINT, SIG_DFL);
	opipe = setsignal(SIGPIPE, SIG_DFL);
	oterm = setsignal(SIGTERM, SIG_DFL);
	ousr1 = setsignal(SIGUSR1, SIG_DFL);
	allsignals(FALSE);

	(void) execve(SaveArgv[0], (ARGV_T) SaveArgv, (ARGV_T) ExternalEnviron);
	save_errno = errno;

	/* restore signals */
	allsignals(TRUE);
	(void) setsignal(SIGALRM, oalrm);
	(void) setsignal(SIGCHLD, ochld);
	(void) setsignal(SIGHUP, ohup);
	(void) setsignal(SIGINT, oint);
	(void) setsignal(SIGPIPE, opipe);
	(void) setsignal(SIGTERM, oterm);
	(void) setsignal(SIGUSR1, ousr1);

	errno = save_errno;
	if (LogLevel > 0)
		sm_syslog(LOG_ALERT, NOQID, "could not exec %s: %m",
			  SaveArgv[0]);
	finis(FALSE, EX_OSFILE);
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

	if (gethostname(hostbuf, size) < 0 || hostbuf[0] == '\0')
		(void) strlcpy(hostbuf, "localhost", size);
	hp = sm_gethostbyname(hostbuf, InetMode);
	if (hp == NULL)
		return NULL;
	if (strchr(hp->h_name, '.') != NULL || strchr(hostbuf, '.') == NULL)
		(void) cleanstrcpy(hostbuf, hp->h_name, size);

# if NETINFO
	if (strchr(hostbuf, '.') == NULL)
	{
		char *domainname;

		domainname = ni_propval("/locations", NULL, "resolver",
					"domain", '\0');
		if (domainname != NULL &&
		    strlen(domainname) + strlen(hostbuf) + 1 < size)
		{
			(void) strlcat(hostbuf, ".", size);
			(void) strlcat(hostbuf, domainname, size);
		}
	}
# endif /* NETINFO */

	/*
	**  If there is still no dot in the name, try looking for a
	**  dotted alias.
	*/

	if (strchr(hostbuf, '.') == NULL)
	{
		char **ha;

		for (ha = hp->h_aliases; ha != NULL && *ha != NULL; ha++)
		{
			if (strchr(*ha, '.') != NULL)
			{
				(void) cleanstrcpy(hostbuf, *ha, size - 1);
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
		(void) sleep(60);
		if (!getcanonname(hostbuf, size, TRUE))
		{
			sm_syslog(LOG_ALERT, NOQID,
				  "unable to qualify my own domain name (%s) -- using short name",
				  hostbuf);
			message("WARNING: unable to qualify my own domain name (%s) -- using short name",
				hostbuf);
		}
	}
	return hp;
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

static int
addrcmp(hp, ha, sa)
	struct hostent *hp;
	char *ha;
	SOCKADDR *sa;
{
# if NETINET6
	u_char *a;
# endif /* NETINET6 */

	switch (sa->sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		if (hp->h_addrtype == AF_INET)
			return memcmp(ha, (char *) &sa->sin.sin_addr, INADDRSZ);
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		a = (u_char *) &sa->sin6.sin6_addr;

		/* Straight binary comparison */
		if (hp->h_addrtype == AF_INET6)
			return memcmp(ha, a, IN6ADDRSZ);

		/* If IPv4-mapped IPv6 address, compare the IPv4 section */
		if (hp->h_addrtype == AF_INET &&
		    IN6_IS_ADDR_V4MAPPED(&sa->sin6.sin6_addr))
			return memcmp(a + IN6ADDRSZ - INADDRSZ, ha, INADDRSZ);
		break;
# endif /* NETINET6 */
	}
	return -1;
}
/*
**  GETAUTHINFO -- get the real host name associated with a file descriptor
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
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(CtxAuthTimeout, 1);
}

char *
getauthinfo(fd, may_be_forged)
	int fd;
	bool *may_be_forged;
{
	volatile u_short port = 0;
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
		if (i < 0)
		{
			/*
			**  ENOTSOCK is OK: bail on anything else, but reset
			**  errno in this case, so a mis-report doesn't
			**  happen later.
			*/
			if (errno != ENOTSOCK)
				return NULL;
			errno = 0;
		}
		(void) snprintf(hbuf, sizeof hbuf, "%s@localhost",
				RealUserName);
		if (tTd(9, 1))
			dprintf("getauthinfo: %s\n", hbuf);
		return hbuf;
	}

	if (RealHostName == NULL)
	{
		/* translate that to a host name */
		RealHostName = newstr(hostnamebyanyaddr(&RealHostAddr));
		if (strlen(RealHostName) > MAXNAME)
			RealHostName[MAXNAME] = '\0';
	}

	/* cross check RealHostName with forward DNS lookup */
	if (anynet_ntoa(&RealHostAddr)[0] == '[' ||
	    RealHostName[0] == '[')
	{
		/*
		**  address is not a socket or have an
		**  IP address with no forward lookup
		*/
		*may_be_forged = FALSE;
	}
	else
	{
		/* try to match the reverse against the forward lookup */
		hp = sm_gethostbyname(RealHostName,
				      RealHostAddr.sa.sa_family);

		if (hp == NULL)
			*may_be_forged = TRUE;
		else
		{
			for (ha = hp->h_addr_list; *ha != NULL; ha++)
				if (addrcmp(hp, *ha, &RealHostAddr) == 0)
					break;
			*may_be_forged = *ha == NULL;
# if _FFR_FREEHOSTENT && NETINET6
			freehostent(hp);
			hp = NULL;
# endif /* _FFR_FREEHOSTENT && NETINET6 */
		}
	}

	if (TimeOuts.to_ident == 0)
		goto noident;

	lalen = sizeof la;
	switch (RealHostAddr.sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		if (getsockname(fd, &la.sa, &lalen) < 0 ||
		    lalen <= 0 ||
		    la.sa.sa_family != AF_INET)
		{
			/* no ident info */
			goto noident;
		}
		port = RealHostAddr.sin.sin_port;

		/* create ident query */
		(void) snprintf(ibuf, sizeof ibuf, "%d,%d\r\n",
				ntohs(RealHostAddr.sin.sin_port),
				ntohs(la.sin.sin_port));

		/* create local address */
		la.sin.sin_port = 0;

		/* create foreign address */
#  ifdef NO_GETSERVBYNAME
		RealHostAddr.sin.sin_port = htons(113);
#  else /* NO_GETSERVBYNAME */
		sp = getservbyname("auth", "tcp");
		if (sp != NULL)
			RealHostAddr.sin.sin_port = sp->s_port;
		else
			RealHostAddr.sin.sin_port = htons(113);
		break;
#  endif /* NO_GETSERVBYNAME */
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		if (getsockname(fd, &la.sa, &lalen) < 0 ||
		    lalen <= 0 ||
		    la.sa.sa_family != AF_INET6)
		{
			/* no ident info */
			goto noident;
		}
		port = RealHostAddr.sin6.sin6_port;

		/* create ident query */
		(void) snprintf(ibuf, sizeof ibuf, "%d,%d\r\n",
				ntohs(RealHostAddr.sin6.sin6_port),
				ntohs(la.sin6.sin6_port));

		/* create local address */
		la.sin6.sin6_port = 0;

		/* create foreign address */
#  ifdef NO_GETSERVBYNAME
		RealHostAddr.sin6.sin6_port = htons(113);
#  else /* NO_GETSERVBYNAME */
		sp = getservbyname("auth", "tcp");
		if (sp != NULL)
			RealHostAddr.sin6.sin6_port = sp->s_port;
		else
			RealHostAddr.sin6.sin6_port = htons(113);
		break;
#  endif /* NO_GETSERVBYNAME */
# endif /* NETINET6 */
	  default:
		/* no ident info */
		goto noident;
	}

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
	s = socket(la.sa.sa_family, SOCK_STREAM, 0);
	if (s < 0)
	{
		clrevent(ev);
		goto noident;
	}
	if (bind(s, &la.sa, lalen) < 0 ||
	    connect(s, &RealHostAddr.sa, lalen) < 0)
	{
		goto closeident;
	}

	if (tTd(9, 10))
		dprintf("getauthinfo: sent %s", ibuf);

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
		dprintf("getauthinfo:  got %s\n", ibuf);

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
	/* put back the original incoming port */
	switch (RealHostAddr.sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		if (port > 0)
			RealHostAddr.sin.sin_port = port;
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		if (port > 0)
			RealHostAddr.sin6.sin6_port = port;
		break;
# endif /* NETINET6 */
	}

	if (RealHostName == NULL)
	{
		if (tTd(9, 1))
			dprintf("getauthinfo: NULL\n");
		return NULL;
	}
	snprintf(hbuf, sizeof hbuf, "%s", RealHostName);

postident:
# if IP_SRCROUTE
#  ifndef GET_IPOPT_DST
#   define GET_IPOPT_DST(dst)	(dst)
#  endif /* ! GET_IPOPT_DST */
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
		struct IPOPTION ipopt;

		ipoptlen = sizeof ipopt;
		if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS,
			       (char *) &ipopt, &ipoptlen) < 0)
			goto noipsr;
		if (ipoptlen == 0)
			goto noipsr;
		o = (u_char *) ipopt.IP_LIST;
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
				    inet_ntoa(GET_IPOPT_DST(ipopt.IP_DST)));
				i = strlen(p);
				p += i;
				l -= strlen(p);

				j = o[1] / sizeof(struct in_addr) - 1;

				/* q skips length and router pointer to data */
				q = &o[3];
				for ( ; j >= 0; j--)
				{
					struct in_addr addr;

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
# endif /* IP_SRCROUTE */
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

# if IP_SRCROUTE
postipsr:
# endif /* IP_SRCROUTE */
	if (tTd(9, 1))
		dprintf("getauthinfo: %s\n", hbuf);

	/* put back the original incoming port */
	switch (RealHostAddr.sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		if (port > 0)
			RealHostAddr.sin.sin_port = port;
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		if (port > 0)
			RealHostAddr.sin6.sin6_port = port;
		break;
# endif /* NETINET6 */
	}

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
# if NETINET
	struct in_addr in_addr;
# endif /* NETINET */
# if NETINET6
	struct in6_addr in6_addr;
# endif /* NETINET6 */
	char *cp, *ans = NULL;
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
			dprintf("host_map_lookup(%s) => CACHE %s\n",
				name,
				s->s_namecanon.nc_cname == NULL
					? "NULL"
					: s->s_namecanon.nc_cname);
		errno = s->s_namecanon.nc_errno;
# if NAMED_BIND
		SM_SET_H_ERRNO(s->s_namecanon.nc_herrno);
# endif /* NAMED_BIND */
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

	if (CurEnv->e_sendmode == SM_DEFER &&
	    bitset(MF_DEFER, map->map_mflags))
	{
		if (tTd(9, 1))
			dprintf("host_map_lookup(%s) => DEFERRED\n", name);
		*statp = EX_TEMPFAIL;
		return NULL;
	}

	/*
	**  If first character is a bracket, then it is an address
	**  lookup.  Address is copied into a temporary buffer to
	**  strip the brackets and to preserve name if address is
	**  unknown.
	*/

	if (tTd(9, 1))
		dprintf("host_map_lookup(%s) => ", name);
	if (*name != '[')
	{
		snprintf(hbuf, sizeof hbuf, "%s", name);
		if (getcanonname(hbuf, sizeof hbuf - 1, !HasWildcardMX))
			ans = hbuf;
	}
	else
	{
		if ((cp = strchr(name, ']')) == NULL)
		{
			if (tTd(9, 1))
				dprintf("FAILED\n");
			return NULL;
		}
		*cp = '\0';

		hp = NULL;
# if NETINET
		if ((in_addr.s_addr = inet_addr(&name[1])) != INADDR_NONE)
			hp = sm_gethostbyaddr((char *)&in_addr,
					      INADDRSZ, AF_INET);
# endif /* NETINET */
# if NETINET6
		if (hp == NULL &&
		    inet_pton(AF_INET6, &name[1], &in6_addr) == 1)
			hp = sm_gethostbyaddr((char *)&in6_addr,
					      IN6ADDRSZ, AF_INET6);
# endif /* NETINET6 */
		*cp = ']';

		if (hp != NULL)
		{
			/* found a match -- copy out */
			ans = denlstring((char *) hp->h_name, TRUE, TRUE);
# if _FFR_FREEHOSTENT && NETINET6
			freehostent(hp);
			hp = NULL;
# endif /* _FFR_FREEHOSTENT && NETINET6 */
		}
	}

	s->s_namecanon.nc_flags |= NCF_VALID;	/* will be soon */

	/* Found an answer */
	if (ans != NULL)
	{
		s->s_namecanon.nc_stat = *statp = EX_OK;
		s->s_namecanon.nc_cname = newstr(ans);
		if (bitset(MF_MATCHONLY, map->map_mflags))
			cp = map_rewrite(map, name, strlen(name), NULL);
		else
			cp = map_rewrite(map, ans, strlen(ans), av);
		if (tTd(9, 1))
			dprintf("FOUND %s\n", ans);
		return cp;
	}


	/* No match found */
	s->s_namecanon.nc_errno = errno;
# if NAMED_BIND
	s->s_namecanon.nc_herrno = h_errno;
	if (tTd(9, 1))
		dprintf("FAIL (%d)\n", h_errno);
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
# else /* NAMED_BIND */
	if (tTd(9, 1))
		dprintf("FAIL\n");
	*statp = EX_NOHOST;
# endif /* NAMED_BIND */
	s->s_namecanon.nc_stat = *statp;
	return NULL;
}
#else /* DAEMON */
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
	if (hostbuf[0] == '\0')
		(void) strlcpy(hostbuf, "localhost", size);
	return NULL;
}
/*
**  GETAUTHINFO -- get the real host name associated with a file descriptor
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
**  HOST_MAP_LOOKUP -- turn a hostname into canonical form
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
	register struct hostent *hp = NULL;
	char *cp;

	hp = sm_gethostbyname(name, InetMode);
	if (hp == NULL && InetMode != AF_INET)
		hp = sm_gethostbyname(name, AF_INET);
	if (hp == NULL)
	{
# if NAMED_BIND
		if (tTd(9, 1))
			dprintf("FAIL (%d)\n", h_errno);
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
#else /* NAMED_BIND */
		*statp = EX_NOHOST;
#endif /* NAMED_BIND */
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		cp = map_rewrite(map, name, strlen(name), NULL);
	else
		cp = map_rewrite(map, hp->h_name, strlen(hp->h_name), avp);
# if _FFR_FREEHOSTENT && NETINET6
	freehostent(hp);
# endif /* _FFR_FREEHOSTENT && NETINET6 */
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

		  case 'S':	/* only for consistency */
			map->map_spacesub = *++p;
			break;

		  case 'D':
			map->map_mflags |= MF_DEFER;
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

#if NETINET6
/*
**  ANYNET_NTOP -- convert an IPv6 network address to printable form.
**
**	Parameters:
**		s6a -- a pointer to an in6_addr structure.
**		dst -- buffer to store result in
**		dst_len -- size of dst buffer
**
**	Returns:
**		A printable version of that structure.
*/
char *
anynet_ntop(s6a, dst, dst_len)
	struct in6_addr *s6a;
	char *dst;
	size_t dst_len;
{
	register char *ap;

	if (IN6_IS_ADDR_V4MAPPED(s6a))
		ap = (char *) inet_ntop(AF_INET,
					&s6a->s6_addr[IN6ADDRSZ - INADDRSZ],
					dst, dst_len);
	else
		ap = (char *) inet_ntop(AF_INET6, s6a, dst, dst_len);
	return ap;
}
#endif /* NETINET6 */
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

# if NETLINK
#  include <net/if_dl.h>
# endif /* NETLINK */

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
# if NETUNIX
	  case AF_UNIX:
		if (sap->sunix.sun_path[0] != '\0')
			snprintf(buf, sizeof buf, "[UNIX: %.64s]",
				sap->sunix.sun_path);
		else
			snprintf(buf, sizeof buf, "[UNIX: localhost]");
		return buf;
# endif /* NETUNIX */

# if NETINET
	  case AF_INET:
		return (char *) inet_ntoa(sap->sin.sin_addr);
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		ap = anynet_ntop(&sap->sin6.sin6_addr, buf, sizeof buf);
		if (ap != NULL)
			return ap;
		break;
# endif /* NETINET6 */

# if NETLINK
	  case AF_LINK:
		snprintf(buf, sizeof buf, "[LINK: %s]",
			link_ntoa((struct sockaddr_dl *) &sap->sa));
		return buf;
# endif /* NETLINK */
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
# if NAMED_BIND
	int saveretry;
# endif /* NAMED_BIND */
# if NETINET6
	struct in6_addr in6_addr;
# endif /* NETINET6 */

# if NAMED_BIND
	/* shorten name server timeout to avoid higher level timeouts */
	saveretry = _res.retry;
	if (_res.retry * _res.retrans > 20)
		_res.retry = 20 / _res.retrans;
# endif /* NAMED_BIND */

	switch (sap->sa.sa_family)
	{
# if NETINET
	  case AF_INET:
		hp = sm_gethostbyaddr((char *) &sap->sin.sin_addr,
			INADDRSZ,
			AF_INET);
		break;
# endif /* NETINET */

# if NETINET6
	  case AF_INET6:
		hp = sm_gethostbyaddr((char *) &sap->sin6.sin6_addr,
				      IN6ADDRSZ,
				      AF_INET6);
		break;
# endif /* NETINET6 */

# if NETISO
	  case AF_ISO:
		hp = sm_gethostbyaddr((char *) &sap->siso.siso_addr,
			sizeof sap->siso.siso_addr,
			AF_ISO);
		break;
# endif /* NETISO */

# if NETUNIX
	  case AF_UNIX:
		hp = NULL;
		break;
# endif /* NETUNIX */

	  default:
		hp = sm_gethostbyaddr(sap->sa.sa_data,
			   sizeof sap->sa.sa_data,
			   sap->sa.sa_family);
		break;
	}

# if NAMED_BIND
	_res.retry = saveretry;
# endif /* NAMED_BIND */

# if NETINET || NETINET6
	if (hp != NULL && hp->h_name[0] != '['
#  if NETINET6
	    && inet_pton(AF_INET6, hp->h_name, &in6_addr) != 1
#  endif /* NETINET6 */
#  if NETINET
	    && inet_addr(hp->h_name) == INADDR_NONE
#  endif /* NETINET */
	    )
	{
		char *name;

		name = denlstring((char *) hp->h_name, TRUE, TRUE);

#  if _FFR_FREEHOSTENT && NETINET6
		if (name == hp->h_name)
		{
			static char n[MAXNAME + 1];

			/* Copy the string, hp->h_name is about to disappear */
			strlcpy(n, name, sizeof n);
			name = n;
		}

		freehostent(hp);
#  endif /* _FFR_FREEHOSTENT && NETINET6 */
		return name;
	}
# endif /* NETINET || NETINET6 */

# if _FFR_FREEHOSTENT && NETINET6
	if (hp != NULL)
	{
		freehostent(hp);
		hp = NULL;
	}
# endif /* _FFR_FREEHOSTENT && NETINET6 */

# if NETUNIX
	if (sap->sa.sa_family == AF_UNIX && sap->sunix.sun_path[0] == '\0')
		return "localhost";
# endif /* NETUNIX */
	{
		static char buf[203];

		(void) snprintf(buf, sizeof buf, "[%.200s]", anynet_ntoa(sap));
		return buf;
	}
}
#endif /* USE_SOCK_STREAM */
