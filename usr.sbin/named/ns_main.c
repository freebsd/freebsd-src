#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_main.c	4.55 (Berkeley) 7/1/91";
static char rcsid[] = "$Id: ns_main.c,v 1.1.1.3 1995/10/23 09:26:19 peter Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1989, 1990
 * -
 * Copyright (c) 1986, 1989, 1990
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if !defined(lint) && !defined(SABER)
char copyright[] =
"@(#) Copyright (c) 1986, 1989, 1990 The Regents of the University of California.\n\
 portions Copyright (c) 1993 Digital Equipment Corporation\n\
 portions Copyright (c) 1993 Berkeley Network Software Consortium\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Internet Name server (see rfc883 & others).
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#if !defined(SYSV) && defined(XXX)
#include <sys/wait.h>
#endif /* !SYSV */
#if defined(__osf__)
# define _SOCKADDR_LEN		/* XXX - should be in portability.h but that
				 * would need to be included before socket.h
				 */
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if defined(__osf__)
# include <sys/mbuf.h>
# include <net/route.h>
#endif
#if defined(_AIX)
# include <sys/time.h>
# define TIME_H_INCLUDED
#endif
#include <net/if.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <resolv.h>
#if defined(SVR4)
# include <sys/sockio.h>
#endif

#define MAIN_PROGRAM
#include "named.h"
#undef MAIN_PROGRAM

#undef nsaddr

				/* UDP receive, TCP send buffer size */
static	const int		rbufsize = 8 * 1024,
				/* TCP send window size */
				sbufsize = 16 * 1024;

static	struct sockaddr_in	nsaddr;
static	u_int16_t		local_ns_port,		/* our service port */
				nsid_state;
static	fd_set			mask;			/* open descriptors */
static	char			**Argv = NULL;
static	char			*LastArg = NULL;	/* end of argv */

static	struct qstream		*sqadd __P((void));
static	void			sq_query __P((struct qstream *)),
				opensocket __P((struct qdatagram *)),
#ifdef DEBUG
				printnetinfo __P((struct netinfo *)),
#endif
				setdebug __P((int));
static	int			sq_here __P((struct qstream *));

static	SIG_FN			onintr __P(()),
				maint_alarm __P(()),
				setdumpflg __P(()),
				onhup __P(()),
#if defined(QRYLOG) && defined(SIGWINCH)
				setQrylogFlg __P(()),
#endif
				setIncrDbgFlg __P(()),
				setNoDbgFlg __P(()),
#ifdef SIGSYS
				sigprof __P(()),
#endif /* SIGSYS */
				setchkptflg __P(()),
				setstatsflg __P(());

static void
usage()
{
	fprintf(stderr,
"Usage: named [-d #] [-q] [-r] [-p port[/localport]] [[-b] bootfile]\n");
	exit(1);
}

/*ARGSUSED*/
void
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	register int n, udpcnt;
	register char *arg;
	register struct qstream *sp;
	register struct qdatagram *dqp;
	struct qstream *nextsp;
	int nfds;
	const int on = 1;
	int rfd, size, len;
	time_t lasttime, maxctime;
	u_char buf[BUFSIZ];
#ifdef POSIX_SIGNALS
	struct sigaction sact;
#else
#ifndef SYSV
	struct sigvec vec;
#endif
#endif
#ifdef NeXT
	int old_sigmask;
#endif
	fd_set tmpmask;
	struct timeval t, *tp;
	struct qstream *candidate = QSTREAM_NULL;
	char **argp;
#ifdef PID_FIX
	char oldpid[10];
#endif
#ifdef WANT_PIDFILE
	FILE	*fp;			/* file descriptor for pid file */
#endif
#ifdef IP_OPTIONS
	u_char ip_opts[50];		/* arbitrary size */
#endif

	local_ns_port = ns_port = htons(NAMESERVER_PORT);

	/* BSD has a better random number generator but it's not clear
	 * that we need it here.
	 */
	gettime(&tt);
	srand(((unsigned)getpid()) + (unsigned)tt.tv_usec);

	/*
	**  Save start and extent of argv for ns_setproctitle().
	*/

	Argv = argp = argv;
	while (*argp)
		argp++;
	LastArg = argp[-1] + strlen(argp[-1]);

	(void) umask(022);
	/* XXX - should use getopt here */
	while (--argc > 0) {
		arg = *++argv;
		if (*arg == '-') {
			while (*++arg)
				switch (*arg) {
				case 'b':
					if (--argc <= 0)
						usage();
					bootfile = savestr(*++argv);
					break;

  				case 'd':
 					++argv;

 					if (*argv != 0) {
 					    if (**argv == '-') {
 						argv--;
 						break;
 					    }
#ifdef DEBUG
 					    debug = atoi(*argv);
#endif
 					    --argc;
 					}
#ifdef DEBUG
					if (debug <= 0)
						debug = 1;
					setdebug(1);
#endif
					break;

				case 'p':
					/* use nonstandard port number.
					 * usage: -p remote/local
					 * remote is the port number to which
					 * we send queries.  local is the port
					 * on which we listen for queries.
					 * local defaults to same as remote.
					 */
					if (--argc <= 0)
						usage();
					ns_port = htons((u_int16_t)
							atoi(*++argv));
					{
					    char *p = strchr(*argv, '/');
					    if (p) {
						local_ns_port =
						    htons((u_int16_t)
							  atoi(p+1));
					    } else {
						local_ns_port = ns_port;
					    }
					}
					break;

#ifdef QRYLOG
				case 'q':
					qrylog = 1;
					break;
#endif

				case 'r':
					NoRecurse = 1;
					break;

				default:
					usage();
				}
		} else
			bootfile = savestr(*argv);
	}

#ifdef DEBUG
	if (!debug)
#endif
		for (n = getdtablesize() - 1; n > 2; n--)
			(void) close(n);	/* don't use my_close() here */
#ifdef DEBUG
	else {
		fprintf(ddt, "Debug turned ON, Level %d\n",debug);
		fprintf(ddt, "Version = %s\n", Version);
		fprintf(ddt, "bootfile = %s\n", bootfile);
	}		
#endif

	n = 0;
#if defined(DEBUG) && defined(LOG_PERROR)
	if (debug)
		n = LOG_PERROR;
#endif
#ifdef LOG_DAEMON
	openlog("named", LOG_PID|LOG_CONS|LOG_NDELAY|n, LOGFAC);
#else
	openlog("named", LOG_PID);
#endif

#ifdef WANT_PIDFILE
	/* tuck my process id away */
#ifdef PID_FIX
	fp = fopen(PidFile, "r+");
	if (fp != NULL) {
		(void) fgets(oldpid, sizeof(oldpid), fp);
		(void) rewind(fp);
		fprintf(fp, "%ld\n", (long)getpid());
		(void) my_fclose(fp);
	}
#else /*PID_FIX*/
	fp = fopen(PidFile, "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		(void) my_fclose(fp);
	}
#endif /*PID_FIX*/
#endif /*WANT_PIDFILE*/

	syslog(LOG_NOTICE, "starting.  %s", Version);

	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);

	nsaddr.sin_family = AF_INET;
	nsaddr.sin_addr.s_addr = INADDR_ANY;
	nsaddr.sin_port = local_ns_port;
	nsid_init();

	/*
	** Open stream port.
	*/
	for (n = 0; ; n++) {
		if ((vs = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_ERR, "socket(SOCK_STREAM): %m");
			exit(1);
		}	
		if (setsockopt(vs, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
			sizeof(on)) != 0)
		{
			syslog(LOG_NOTICE, "setsockopt(vs, reuseaddr): %m");
			(void) my_close(vs);
			continue;
		}
		if (bind(vs, (struct sockaddr *)&nsaddr, sizeof(nsaddr)) == 0)
			break;

		if (errno != EADDRINUSE || n > 4) {
			if (errno == EADDRINUSE) {
				syslog(LOG_NOTICE,
				 "There may be a name server already running");
				syslog(LOG_ERR, "exiting");
			} else {
				syslog(LOG_ERR, "bind(vs, [%s].%d): %m",
					inet_ntoa(nsaddr.sin_addr),
					ntohs(nsaddr.sin_port));
			}
#if defined(WANT_PIDFILE) && defined(PID_FIX)
			/* put old pid back */
			if (atoi(oldpid) && (fp = fopen(PidFile, "w"))) {
				fprintf(fp, "%s", oldpid);
				(void) my_fclose(fp);
				_exit(1);
			}
#endif /*WANT_PIDFILE && PID_FIX*/
			exit(1);
		}
		/* Retry opening the socket a few times */
		my_close(vs);
		sleep(3);
	}
	if (listen(vs, 5) != 0) {
		syslog(LOG_ERR, "listen(vs, 5): %m");
		exit(1);
	}

  	/*
	 * named would be terminated if one of these is sent and no handler.
	 */
	(void) signal(SIGINT, setdumpflg);
	(void) signal(SIGQUIT, setchkptflg);
	(void) signal(SIGIOT, setstatsflg);
#if defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, setIncrDbgFlg);
	(void) signal(SIGUSR2, setNoDbgFlg);
#else /* SIGUSR1&&SIGUSR2 */
	(void) signal(SIGEMT, setIncrDbgFlg);
	(void) signal(SIGFPE, setNoDbgFlg);
#endif /* SIGUSR1&&SIGUSR2 */

#if defined(SIGWINCH) && defined(QRYLOG)
	(void) signal(SIGWINCH, setQrylogFlg);
#endif

	/*
	 * Get list of local addresses and set up datagram sockets.
	 */
	FD_ZERO(&mask);
	FD_SET(vs, &mask);
	getnetconf();

	/*
	** Initialize and load database.
	*/
	gettime(&tt);
	buildservicelist();
	buildprotolist();
	ns_init(bootfile);
#ifdef DEBUG
	if (debug) {
		fprintf(ddt, "Network and sort list:\n");
		printnetinfo(nettab);
	}
#endif

	time(&boottime);
	resettime = boottime;

	(void) signal(SIGHUP, onhup);
#if defined(SIGXFSZ)
	(void) signal(SIGXFSZ, onhup);	/* wierd DEC Hesiodism, harmless */
#endif
#if defined(POSIX_SIGNALS)
	bzero((char *)&sact, sizeof(sact));
	sact.sa_handler = maint_alarm;
	sigemptyset(&sact.sa_mask);
	sigaddset(&sact.sa_mask, SIGCHLD);
	(void) sigaction(SIGALRM, &sact, (struct sigaction *)NULL);

	sact.sa_handler = endxfer;
	sigemptyset(&sact.sa_mask);
	sigaddset(&sact.sa_mask, SIGALRM);
	(void) sigaction(SIGCHLD, &sact, (struct sigaction *)NULL);
#else 
#if defined(SYSV)
	(void) signal(SIGCLD, (SIG_FN (*)()) endxfer);
	(void) signal(SIGALRM, maint_alarm);
#else
	bzero((char *)&vec, sizeof(vec));
	vec.sv_handler = maint_alarm;
	vec.sv_mask = sigmask(SIGCHLD);
	(void) sigvec(SIGALRM, &vec, (struct sigvec *)NULL);

	vec.sv_handler = endxfer;
	vec.sv_mask = sigmask(SIGALRM);
	(void) sigvec(SIGCHLD, &vec, (struct sigvec *)NULL);
#endif /* SYSV */
#endif /* POSIX_SIGNALS */
	(void) signal(SIGPIPE, SIG_IGN);
#ifdef SIGSYS
	(void) signal(SIGSYS, sigprof);
#endif /* SIGSYS */

#ifdef ALLOW_UPDATES
        /* Catch SIGTERM so we can dump the database upon shutdown if it
           has changed since it was last dumped/booted */
        (void) signal(SIGTERM, onintr);
#endif

#ifdef XSTATS
        /* Catch SIGTERM so we can write stats before exiting. */
        (void) signal(SIGTERM, onintr);
#endif

	dprintf(1, (ddt, "database initialized\n"));
	t.tv_usec = 0;

	/*
	 * Fork and go into background now that
	 * we've done any slow initialization
	 * and are ready to answer queries.
	 */
#ifdef USE_SETSID
	if (
#ifdef DEBUG
	    !debug ||
#endif
	    !isatty(0)) {
		if (fork() > 0)
			exit(0);
		setsid();
#ifdef DEBUG
		if (!debug)
#endif
		{
			n = open(_PATH_DEVNULL, O_RDONLY);
			(void) dup2(n, 0);
			(void) dup2(n, 1);
			(void) dup2(n, 2);
			if (n > 2)
				(void) my_close(n);
		}
	}
#else
#ifdef DEBUG
	if (!debug)
#endif
	{
#ifdef HAVE_DAEMON
		daemon(1, 0);
#else
		switch (fork()) {
		case -1:
			syslog(LOG_ERR, "fork: %m");
			exit(1);
			/*FALLTHROUGH*/
		case 0:
			/* child */
			break;
		default:
			/* parent */
			exit(0);
		}
		n = open(_PATH_DEVNULL, O_RDONLY);
		(void) dup2(n, 0);
		(void) dup2(n, 1);
		(void) dup2(n, 2);
		if (n > 2)
			(void) my_close(n);
#if defined(SYSV) || defined(hpux)
		setpgrp();
#else
		{
			struct itimerval ival;

			/*
			 * The open below may hang on pseudo ttys if the person
			 * who starts named logs out before this point.
			 *
			 * needmaint may get set inapropriately if the open
			 * hangs, but all that will happen is we will see that
			 * no maintenance is required.
			 */
			bzero((char *)&ival, sizeof(ival));
			ival.it_value.tv_sec = 120;
			(void) setitimer(ITIMER_REAL, &ival,
				    (struct itimerval *)NULL);
			n = open(_PATH_TTY, O_RDWR);
			ival.it_value.tv_sec = 0;
			(void) setitimer(ITIMER_REAL, &ival,
				    (struct itimerval *)NULL);
			if (n > 0) {
				(void) ioctl(n, TIOCNOTTY, (char *)NULL);
				(void) my_close(n);
			}
		}
#endif /* SYSV */
#endif /* HAVE_DAEMON */
	}
#endif /* USE_SETSID */
#ifdef WANT_PIDFILE
	/* tuck my process id away again */
	fp = fopen(PidFile, "w");
	if (fp != NULL) {
		fprintf(fp, "%ld\n", (long)getpid());
		(void) my_fclose(fp);
	}
#endif

	syslog(LOG_NOTICE, "Ready to answer queries.\n");
	prime_cache();
	nfds = getdtablesize();       /* get the number of file descriptors */
	if (nfds > FD_SETSIZE) {
		nfds = FD_SETSIZE;	/* Bulletproofing */
		syslog(LOG_NOTICE, "Return from getdtablesize() > FD_SETSIZE");
	}
#ifdef NeXT
	old_sigmask = sigblock(sigmask(SIGCHLD));
#endif
	for (;;) {
#ifdef DEBUG
		if (ddt && debug == 0) {
			fprintf(ddt,"Debug turned OFF\n");
			(void) my_fclose(ddt);
			ddt = 0;
		}
#endif
#ifdef ALLOW_UPDATES
                if (needToExit) {
			struct zoneinfo *zp;
			sigblock(~0);   /*
					 * Block all blockable signals
					 * to ensure a consistant
					 * state during final dump
					 */
			dprintf(1, (ddt, "Received shutdown signal\n"));
			for (zp = zones; zp < &zones[nzones]; zp++) {
				if (zp->z_flags & Z_CHANGED)
					zonedump(zp);
                        }
                        exit(0);
                }
#endif /* ALLOW_UPDATES */
#ifdef XSTATS
                if (needToExit) {
		  	ns_logstats();
		  	exit(0);
                }
#endif /* XSTATS */
		if (needreload) {
			needreload = 0;
			db_reload();
		}
		if (needStatsDump) {
			needStatsDump = 0;
			ns_stats();
		}
		if (needzoneload) {
			needzoneload = 0;
			loadxfer();
		}
		if (needmaint) {
                        needmaint = 0;
                        ns_maint();
                }
	        if(needToChkpt) {
                        needToChkpt = 0;
                        doachkpt();
	        }
                if(needToDoadump) {
                        needToDoadump = 0;
                        doadump();
                }
		/*
		** Wait until a query arrives
		*/
		if (retryqp != NULL) {
			gettime(&tt);
			/*
			** The tv_sec field might be unsigned 
			** and thus cannot be negative.
			*/
			if ((int32_t) retryqp->q_time <= tt.tv_sec) {
				retry(retryqp);
				continue;
			}
			t.tv_sec = (int32_t) retryqp->q_time - tt.tv_sec;
			tp = &t;
		} else
			tp = NULL;
		tmpmask = mask;
#ifdef NeXT
		sigsetmask(old_sigmask);	/* Let queued signals run. */
#endif
		n = select(nfds, &tmpmask, (fd_set *)NULL, (fd_set *)NULL, tp);
#ifdef NeXT
		old_sigmask = sigblock(sigmask(SIGCHLD));
#endif
		if (n < 0 && errno != EINTR) {
			syslog(LOG_ERR, "select: %m");
			sleep(60);
		}
		if (n <= 0)
			continue;

		for (dqp = datagramq;
		     dqp != QDATAGRAM_NULL;
		     dqp = dqp->dq_next) {
		    if (FD_ISSET(dqp->dq_dfd, &tmpmask))
		        for (udpcnt = 0; udpcnt < 42; udpcnt++) {  /*XXX*/
			    int from_len = sizeof(from_addr);

			    if ((n = recvfrom(dqp->dq_dfd, (char *)buf, sizeof(buf), 0,
				(struct sockaddr *)&from_addr, &from_len)) < 0)
			    {
#if defined(SPURIOUS_ECONNREFUSED)
				if ((n < 0) && (errno == ECONNREFUSED))
					break;
#endif
				if ((n < 0) && (errno == PORT_WOULDBLK))
					break;
				syslog(LOG_INFO, "recvfrom: %m");
				break;
			    }
			    if (n == 0)
				break;
			    gettime(&tt);
			    dprintf(1, (ddt,
			     "\ndatagram from [%s].%d, fd %d, len %d; now %s",
					inet_ntoa(from_addr.sin_addr),
					ntohs(from_addr.sin_port),
					dqp->dq_dfd, n,
				        ctimel(tt.tv_sec)));
#ifdef DEBUG
			    if (debug >= 10)
				fp_nquery(buf, n, ddt);
#endif
			    /*
			     * Consult database to get the answer.
			     */
			    gettime(&tt);
			    ns_req(buf, n, PACKETSZ, QSTREAM_NULL, &from_addr,
				    dqp->dq_dfd);
		        }
		}
		/*
		** Process stream connection.
		**
		** Note that a "continue" in here takes us back to the select()
		** which, if our accept() failed, will bring us back here.
		*/
		if (FD_ISSET(vs, &tmpmask)) {
			int from_len = sizeof(from_addr);

			rfd = accept(vs,
				     (struct sockaddr *)&from_addr,
				     &from_len);
			if (rfd < 0 && errno == EINTR)
				continue;
			if (rfd < 0 && errno == EMFILE && streamq) {
				maxctime = 0;
				candidate = NULL;
				for (sp = streamq; sp; sp = nextsp) {
					nextsp = sp->s_next;
					if (sp->s_refcnt)
						continue;
					gettime(&tt);
					lasttime = tt.tv_sec - sp->s_time;
					if (lasttime >= VQEXPIRY)
						sqrm(sp);
					else if (lasttime > maxctime) {
						candidate = sp;
						maxctime = lasttime;
					}
				}
				if (candidate)
					sqrm(candidate);
				continue;
			}
			if (rfd < 0) {
				syslog(LOG_INFO, "accept: %m");
				continue;
			}
			if ((n = fcntl(rfd, F_GETFL, 0)) < 0) {
				syslog(LOG_INFO, "fcntl(rfd, F_GETFL): %m");
				(void) my_close(rfd);
				continue;
			}
			if (fcntl(rfd, F_SETFL, n|PORT_NONBLOCK) != 0) {
				syslog(LOG_INFO, "fcntl(rfd, NONBLOCK): %m");
				(void) my_close(rfd);
				continue;
			}
#if defined(IP_OPTIONS)
			len = sizeof ip_opts;
			if (getsockopt(rfd, IPPROTO_IP, IP_OPTIONS,
				       (char *)ip_opts, &len) < 0) {
				syslog(LOG_INFO,
				       "getsockopt(rfd, IP_OPTIONS): %m");
				(void) my_close(rfd);
				continue;
			}
			if (len != 0) {
				nameserIncr(from_addr.sin_addr, nssRcvdOpts);
				if (!haveComplained((char*)
						    from_addr.sin_addr.s_addr,
						    "rcvd ip options")) {
					syslog(LOG_INFO,
				      "rcvd IP_OPTIONS from [%s].%d (ignored)",
					       inet_ntoa(from_addr.sin_addr),
					       ntohs(from_addr.sin_port));
				}
				if (setsockopt(rfd, IPPROTO_IP, IP_OPTIONS,
					       NULL, 0) < 0) {
					syslog(LOG_INFO,
					       "setsockopt(!IP_OPTIONS): %m");
					(void) my_close(rfd);
					continue;
				}
			}
#endif
			if (setsockopt(rfd, SOL_SOCKET, SO_SNDBUF,
				      (char*)&sbufsize, sizeof(sbufsize)) < 0){
				syslog(LOG_INFO,
					  "setsockopt(rfd, SO_SNDBUF, %d): %m",
				       sbufsize);
				(void) my_close(rfd);
				continue;
			}
			if (setsockopt(rfd, SOL_SOCKET, SO_KEEPALIVE,
				       (char *)&on, sizeof(on)) < 0) {
				syslog(LOG_INFO,
				       "setsockopt(rfd, KEEPALIVE): %m");
				(void) my_close(rfd);
				continue;
			}
			if ((sp = sqadd()) == QSTREAM_NULL) {
				(void) my_close(rfd);
				continue;
			}
			sp->s_rfd = rfd;	/* stream file descriptor */
			sp->s_size = -1;	/* amount of data to receive */
			gettime(&tt);
			sp->s_time = tt.tv_sec;	/* last transaction time */
			sp->s_from = from_addr;	/* address to respond to */
			sp->s_bufp = (u_char *)&sp->s_tempsize;
			FD_SET(rfd, &mask);
			FD_SET(rfd, &tmpmask);
			dprintf(1, (ddt,
				  "\nTCP connection from [%s].%d (fd %d)\n",
				    inet_ntoa(sp->s_from.sin_addr),
				    ntohs(sp->s_from.sin_port), rfd));
		}
		if (streamq)
			dprintf(3, (ddt, "streamq = 0x%lx\n",
				    (u_long)streamq));
		for (sp = streamq;  sp != QSTREAM_NULL;  sp = nextsp) {
			nextsp = sp->s_next;
			if (!FD_ISSET(sp->s_rfd, &tmpmask))
				continue;
			dprintf(5, (ddt,
				  "sp x%lx rfd %d size %d time %d next x%lx\n",
				    (u_long)sp, sp->s_rfd, sp->s_size,
				    sp->s_time, (u_long)sp->s_next));
			dprintf(5, (ddt,
				    "\tbufsize %d buf x%lx bufp x%lx\n",
				    sp->s_bufsize,
				    (u_long)sp->s_buf, (u_long)sp->s_bufp));
			if (sp->s_size < 0) {
				size = INT16SZ
				    - (sp->s_bufp - (u_char *)&sp->s_tempsize);
			        while (size > 0 &&
			           (n = read(sp->s_rfd, sp->s_bufp, size)) > 0
				       ) {
					sp->s_bufp += n;
					size -= n;
			        }
				if ((n < 0) && (errno == PORT_WOULDBLK))
					continue;
				if (n <= 0) {
					sqrm(sp);
					continue;
			        }
			        if ((sp->s_bufp - (u_char *)&sp->s_tempsize) ==
					INT16SZ) {
					sp->s_size = htons(sp->s_tempsize);
					if (sp->s_bufsize == 0) {
					    if ( (sp->s_buf = (u_char *)
						  malloc(rbufsize))
						== NULL) {
						    sp->s_buf = buf;
						    sp->s_size  = sizeof(buf);
					    } else {
						    sp->s_bufsize = rbufsize;
					    }
					}
					if (sp->s_size > sp->s_bufsize &&
					    sp->s_bufsize != 0
					) {
					    sp->s_buf = (u_char *)
						realloc((char *)sp->s_buf,
							(unsigned)sp->s_size);
					    if (sp->s_buf == NULL) {
						sp->s_buf = buf;
						sp->s_bufsize = 0;
						sp->s_size = sizeof(buf);
					    } else {
						sp->s_bufsize = sp->s_size;
					    }
					}
					sp->s_bufp = sp->s_buf;	
				}
			}
			gettime(&tt);
			sp->s_time = tt.tv_sec;
			while (sp->s_size > 0 &&
			      (n = read(sp->s_rfd,
					sp->s_bufp,
					sp->s_size)
			       ) > 0
			) {
				sp->s_bufp += n;
				sp->s_size -= n;
			}
			/*
			 * we don't have enough memory for the query.
			 * if we have a query id, then we will send an
			 * error back to the user.
			 */
			if (sp->s_bufsize == 0 &&
			    (sp->s_bufp - sp->s_buf > INT16SZ)) {
				HEADER *hp;

				hp = (HEADER *)sp->s_buf;
				hp->qr = 1;
				hp->ra = (NoRecurse == 0);
				hp->ancount = 0;
				hp->qdcount = 0;
				hp->nscount = 0;
				hp->arcount = 0;
				hp->rcode = SERVFAIL;
				(void) writemsg(sp->s_rfd, sp->s_buf,
						HFIXEDSZ);
				continue;
			}
			if ((n == -1) && (errno == PORT_WOULDBLK))
				continue;
			if (n <= 0) {
				sqrm(sp);
				continue;
			}
			/*
			 * Consult database to get the answer.
			 */
			if (sp->s_size == 0) {
				nameserIncr(sp->s_from.sin_addr, nssRcvdTCP);
				sq_query(sp);
				ns_req(sp->s_buf,
				       sp->s_bufp - sp->s_buf,
				       sp->s_bufsize, sp,
				       &sp->s_from, -1);
				/* ns_req() can call sqrm() - check for it */
				if (sq_here(sp)) {
					sp->s_bufp = (u_char *)&sp->s_tempsize;
					sp->s_size = -1;
				}
				continue;
			}
		}
	}
	/* NOTREACHED */
}

void
getnetconf()
{
	register struct netinfo *ntp;
	struct netinfo *ontp;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct qdatagram *dqp;
	static int first = 1;
	char buf[32768], *cp, *cplim;
	u_int32_t nm;
	time_t my_generation = time(NULL);

	ifc.ifc_len = sizeof buf;
	ifc.ifc_buf = buf;
	if (ioctl(vs, SIOCGIFCONF, (char *)&ifc) < 0) {
		syslog(LOG_ERR, "get interface configuration: %m - exiting");
		exit(1);
	}
	ntp = NULL;
#if defined(AF_LINK) && !defined(RISCOS_BSD)
#define my_max(a, b) (a > b ? a : b)
#define my_size(p)	my_max((p).sa_len, sizeof(p))
#else
#define my_size(p) (sizeof (p))
#endif
	cplim = buf + ifc.ifc_len;    /* skip over if's with big ifr_addr's */
	for (cp = buf;
	     cp < cplim;
	     cp += sizeof (ifr->ifr_name) + my_size(ifr->ifr_addr)) {
#undef my_size
		ifr = (struct ifreq *)cp;
		if (ifr->ifr_addr.sa_family != AF_INET ||
		   ((struct sockaddr_in *)
		    &ifr->ifr_addr)->sin_addr.s_addr == 0) {
			continue;
		}
		ifreq = *ifr;
		/*
		 * Don't test IFF_UP, packets may still be received at this
		 * address if any other interface is up.
		 */
#if !defined(BSD) || (BSD < 199103)
		if (ioctl(vs, SIOCGIFADDR, (char *)&ifreq) < 0) {
			syslog(LOG_NOTICE, "get interface addr: %m");
			continue;
		}
#endif
		dprintf(1, (ddt, "considering [%s]\n",
			    inet_ntoa(((struct sockaddr_in *)
				       &ifreq.ifr_addr)->sin_addr)));
		/* build datagram queue */
		/* 
		 * look for an already existing source interface address.
		 * This happens mostly when reinitializing.  Also, if
		 * the machine has multiple point to point interfaces, then 
		 * the local address may appear more than once.
		 */
		if (dqp = aIsUs(((struct sockaddr_in *)&ifreq.ifr_addr)
				->sin_addr)) {
			dprintf(1, (ddt,
				    "dup interface address %s on %s\n",
				    inet_ntoa(((struct sockaddr_in *)
					       &ifreq.ifr_addr)->sin_addr),
				    ifreq.ifr_name));
			dqp->dq_gen = my_generation;
			continue;
		}

		/*
		 * Skip over address 0.0.0.0 since this will conflict
		 * with binding to wildcard address later.  Interfaces
		 * which are not completely configured can have this addr.
		 */
		if (((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr
		    == 0x00000000) {	/* XXX */
			dprintf(1, (ddt, "skipping address 0.0.0.0 on %s\n",
				    ifreq.ifr_name));
			continue;
		}
		if ((dqp = (struct qdatagram *)
			   calloc(1, sizeof(struct qdatagram))
		     ) == NULL) {
			syslog(LOG_ERR, "getnetconf: malloc: %m");
			exit(12);
		}
		dqp->dq_next = datagramq;
		datagramq = dqp;
		dqp->dq_addr = ((struct sockaddr_in *)
				&ifreq.ifr_addr)->sin_addr;
		dqp->dq_gen = my_generation;
		opensocket(dqp);
		dprintf(1, (ddt, "listening [%s]\n",
			    inet_ntoa(((struct sockaddr_in *)
				       &ifreq.ifr_addr)->sin_addr)));

		/*
		 * Add interface to list of directly-attached (sub)nets
		 * for use in sorting addresses.
		 */
		if (ntp == NULL) {
			ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
		}
		ntp->my_addr = ((struct sockaddr_in *)
				&ifreq.ifr_addr)->sin_addr;
#ifdef SIOCGIFNETMASK
		if (ioctl(vs, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			syslog(LOG_NOTICE, "get netmask: %m");
			ntp->mask = net_mask(ntp->my_addr);
		} else
			ntp->mask = ((struct sockaddr_in *)
			    &ifreq.ifr_addr)->sin_addr.s_addr;
#else
		/* 4.2 does not support subnets */
		ntp->mask = net_mask(ntp->my_addr);
#endif
		if (ioctl(vs, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			syslog(LOG_NOTICE, "get interface flags: %m");
			continue;
		}
#ifdef IFF_LOOPBACK
		if (ifreq.ifr_flags & IFF_LOOPBACK)
#else
		/* test against 127.0.0.1 (yuck!!) */
		if (ntp->my_addr.s_addr == inet_addr("127.0.0.1"))  /* XXX */
#endif
		{
			if (netloop.my_addr.s_addr == 0) {
				netloop.my_addr = ntp->my_addr;
				netloop.mask = 0xffffffff;
				netloop.addr = ntp->my_addr.s_addr;
				dprintf(1, (ddt, "loopback address: x%lx\n",
					    netloop.my_addr.s_addr));
			}
			continue;
		} else if ((ifreq.ifr_flags & IFF_POINTOPOINT)) {
			if (ioctl(vs, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
				syslog(LOG_NOTICE, "get dst addr: %m");
				continue;
			}
			ntp->mask = 0xffffffff;
			ntp->addr = ((struct sockaddr_in *)
				     &ifreq.ifr_addr)->sin_addr.s_addr;
		} else {
			ntp->addr = ntp->mask & ntp->my_addr.s_addr;
		}
		/*
		 * Place on end of list of locally-attached (sub)nets,
		 * but before logical nets for subnetted nets.
		 */
		ntp->next = *elocal;
		*elocal = ntp;
		if (elocal == enettab)
			enettab = &ntp->next;
		elocal = &ntp->next;
		ntp = NULL;
	}
	if (ntp)
		free((char *)ntp);

	/*
	 * now go through the datagramq and delete anything that
	 * does not have the current generation number.  this is
	 * how we catch interfaces that go away or change their
	 * addresses.  note that 0.0.0.0 is the wildcard element
	 * and should never be deleted by this code.
	 *
	 * XXX - need to update enettab/elocal as well.
	 */
	dqflush(my_generation);		/* With apologies to The Who. */

	/*
	 * Create separate qdatagram structure for socket
	 * wildcard address.
	 */
	if (first) {
		if (!(dqp = (struct qdatagram *)calloc(1, sizeof(*dqp)))) {
			syslog(LOG_ERR, "getnetconf: malloc: %m");
			exit(12);
		}
		dqp->dq_next = datagramq;
		datagramq = dqp;
		dqp->dq_addr.s_addr = INADDR_ANY;
		opensocket(dqp);
		ds = dqp->dq_dfd;
	}

	/*
	 * Compute logical networks to which we're connected
	 * based on attached subnets;
	 * used for sorting based on network configuration.
	 */
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		nm = net_mask(ntp->my_addr);
		if (nm != ntp->mask) {
			if (findnetinfo(ntp->my_addr))
				continue;
			ontp = (struct netinfo *)
			       malloc(sizeof(struct netinfo));
			if (ontp == NULL) {
				syslog(LOG_ERR, "getnetconf: malloc: %m");
				exit(12);
			}
			ontp->my_addr = ntp->my_addr;
			ontp->mask = nm;
			ontp->addr = ontp->my_addr.s_addr & nm;
			ontp->next = *enettab;
			*enettab = ontp;
			enettab = &ontp->next;
		}
	}
	first = 0;
}

/*
 * Find netinfo structure for logical network implied by address "addr",
 * if it's on list of local/favored networks.
 */
struct netinfo *
findnetinfo(addr)
	struct in_addr addr;
{
	register struct netinfo *ntp;
	u_int32_t net, mask;

	mask = net_mask(addr);
	net = addr.s_addr & mask;
	for (ntp = nettab; ntp != NULL; ntp = ntp->next)
		if (ntp->addr == net && ntp->mask == mask)
			return (ntp);
	return ((struct netinfo *) NULL);
}

#ifdef DEBUG
static void
printnetinfo(ntp)
	register struct netinfo *ntp;
{
	for ( ; ntp != NULL; ntp = ntp->next) {
		fprintf(ddt, "addr x%lx mask x%lx",
			(u_long)ntp->addr, (u_long)ntp->mask);
		fprintf(ddt, " my_addr x%lx", ntp->my_addr.s_addr);
		fprintf(ddt, " %s\n", inet_ntoa(ntp->my_addr));
	}
}
#endif

static void
opensocket(dqp)
	register struct qdatagram *dqp;
{
	int m, n;
	int on = 1;

	/*
	 * Open datagram sockets bound to interface address.
	 */
	if ((dqp->dq_dfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket(SOCK_DGRAM): %m - exiting");
		exit(1);
	}	
	dprintf(1, (ddt, "dqp->dq_addr %s d_dfd %d\n",
		    inet_ntoa(dqp->dq_addr), dqp->dq_dfd));
	if (setsockopt(dqp->dq_dfd, SOL_SOCKET, SO_REUSEADDR,
	    (char *)&on, sizeof(on)) != 0)
	{
		syslog(LOG_NOTICE, "setsockopt(dqp->dq_dfd, reuseaddr): %m");
		/* XXX press on regardless, this is not too serious. */
	}
#ifdef SO_RCVBUF
	m = sizeof(n);
	if ((getsockopt(dqp->dq_dfd, SOL_SOCKET, SO_RCVBUF, (char*)&n, &m) >= 0)
	    && (m == sizeof(n))
	    && (n < rbufsize)) {
		(void) setsockopt(dqp->dq_dfd, SOL_SOCKET, SO_RCVBUF,
				  (char *)&rbufsize, sizeof(rbufsize));
	}
#endif /* SO_RCVBUF */
	if ((n = fcntl(dqp->dq_dfd, F_GETFL, 0)) < 0) {
		syslog(LOG_NOTICE, "fcntl(dfd, F_GETFL): %m");
		/* XXX press on regardless, but this really is a problem. */
	} else if (fcntl(dqp->dq_dfd, F_SETFL, n|PORT_NONBLOCK) != 0) {
		syslog(LOG_NOTICE, "fcntl(dqp->dq_dfd, non-blocking): %m");
		/* XXX press on regardless, but this really is a problem. */
	}
	/*
	 *   NOTE: Some versions of SunOS have problems with the following
	 *   call to bind.  Bind still seems to function on these systems
	 *   if you comment out the exit inside the if.  This may cause
	 *   Suns with multiple interfaces to reply strangely.
	 */
	nsaddr.sin_addr = dqp->dq_addr;
	if (bind(dqp->dq_dfd, (struct sockaddr *)&nsaddr, sizeof(nsaddr))) {
		syslog(LOG_NOTICE, "bind(dfd=%d, [%s].%d): %m",
			dqp->dq_dfd, inet_ntoa(nsaddr.sin_addr),
			ntohs(nsaddr.sin_port));
#if !defined(sun)
		syslog(LOG_ERR, "exiting");
		exit(1);
#endif
	}
	FD_SET(dqp->dq_dfd, &mask);
}

/*
** Set flag saying to reload database upon receiving SIGHUP.
** Must make sure that someone isn't walking through a data
** structure at the time.
*/

static SIG_FN
onhup()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGHUP, (SIG_FN (*)())onhup);
#endif /* SYSV */
	needreload = 1;
	errno = save_errno;
}

/*
** Set flag saying to call ns_maint()
** Must make sure that someone isn't walking through a data
** structure at the time.
*/

static SIG_FN
maint_alarm()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGALRM, (SIG_FN (*)())maint_alarm);
#endif /* SYSV */
	needmaint = 1;
	errno = save_errno;
}


#ifdef ALLOW_UPDATES
/*
 * Signal handler to schedule shutdown.  Just set flag, to ensure a consistent
 * state during dump.
 */
static SIG_FN
onintr()
{
        needToExit = 1;
}
#endif /* ALLOW_UPDATES */

#ifdef XSTATS
/*
 * Signal handler to write log information
 */
static SIG_FN
onintr()
{
        needToExit = 1;
}
#endif /* XSTATS */

/*
 * Signal handler to schedule a data base dump.  Do this instead of dumping the
 * data base immediately, to avoid seeing it in a possibly inconsistent state
 * (due to updates), and to avoid long disk I/O delays at signal-handler
 * level
 */
static SIG_FN
setdumpflg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGINT, (SIG_FN (*)())setdumpflg);
#endif /* SYSV */
        needToDoadump = 1;
	errno = save_errno;
}

/*
** Turn on or off debuging by open or closeing the debug file
*/

static void
setdebug(code)
	int code;
{
#if defined(lint) && !defined(DEBUG)
	code = code;
#endif
#ifdef DEBUG

	if (code) {
		int n;

		ddt = freopen(debugfile, "w+", stderr);
		if ( ddt == NULL) {
			syslog(LOG_NOTICE, "can't open debug file %s: %m",
			       debugfile);
			debug = 0;
		} else {
#if defined(HAVE_SETVBUF)
			setvbuf(ddt, NULL, _IOLBF, BUFSIZ);
#else
			setlinebuf(ddt);
#endif
			if ((n = fcntl(fileno(ddt), F_GETFL, 0)) < 0) {
				syslog(LOG_INFO,
				       "fcntl(ddt, F_GETFL): %m");
			} else {
				(void) fcntl(fileno(ddt), F_SETFL, n|O_APPEND);
			}
		}
	} else
		debug = 0;
		/* delay closing ddt, we might interrupt someone */
#endif
}

/*
** Catch a special signal and set debug level.
**
**  If debuging is off then turn on debuging else increment the level.
**
** Handy for looking in on long running name servers.
*/

static SIG_FN
setIncrDbgFlg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGUSR1, (SIG_FN (*)())setIncrDbgFlg);
#endif /* SYSV */
#ifdef DEBUG
	if (debug == 0) {
		debug++;
		setdebug(1);
	} else {
		debug++;
	}
	if (debug)
		fprintf(ddt, "Debug turned ON, Level %d\n", debug);
#endif
	errno = save_errno;
}

/*
** Catch a special signal to turn off debugging
*/

static SIG_FN
setNoDbgFlg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGUSR2, (SIG_FN (*)())setNoDbgFlg);
#endif /* SYSV */
	setdebug(0);
	errno = save_errno;
}

#if defined(QRYLOG) && defined(SIGWINCH)
/*
** Set flag for query logging
*/
static SIG_FN
setQrylogFlg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGWINCH, (SIG_FN (*)())setQrylogFlg);
#endif /* SYSV */
	qrylog = !qrylog;
	syslog(LOG_NOTICE, "query log %s\n", qrylog ?"on" :"off");
	errno = save_errno;
}
#endif /*QRYLOG && SIGWINCH*/

/*
** Set flag for statistics dump
*/
static SIG_FN
setstatsflg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGIOT, (SIG_FN (*)())setstatsflg);
#endif /* SYSV */
	needStatsDump = 1;
	errno = save_errno;
}

static SIG_FN
setchkptflg()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGQUIT, (SIG_FN (*)())setchkptflg);
#endif /* SYSV */
	needToChkpt = 1;
	errno = save_errno;
}

/*
** Catch a special signal SIGSYS
**
**  this is setup to fork and exit to drop to /usr/tmp/gmon.out
**   and keep the server running
*/

#ifdef SIGSYS
static SIG_FN
sigprof()
{
	int save_errno = errno;
#if defined(SYSV)
	(void)signal(SIGSYS, (SIG_FN (*)())sigprof);
#endif /* SYSV */
	dprintf(1, (ddt, "sigprof()\n"));
	if (fork() == 0)
	{
		(void) chdir(_PATH_TMPDIR);
		exit(1);
	}
	errno = save_errno;
}
#endif /* SIGSYS */

/*
** Routines for managing stream queue
*/

static struct qstream *
sqadd()
{
	register struct qstream *sqp;

	if (!(sqp = (struct qstream *)calloc(1, sizeof(struct qstream)))) {
		syslog(LOG_ERR, "sqadd: calloc: %m");
		return (QSTREAM_NULL);
	}
	dprintf(3, (ddt, "sqadd(x%lx)\n", (u_long)sqp));

	sqp->s_next = streamq;
	streamq = sqp;
	return (sqp);
}

/* sqrm(qp)
 *	remove stream queue structure `qp'.
 *	no current queries may refer to this stream when it is removed.
 * side effects:
 *	memory is deallocated.  sockets are closed.  lists are relinked.
 */
void
sqrm(qp)
	register struct qstream *qp;
{
	register struct qstream *qsp;

	dprintf(2, (ddt, "sqrm(%#lx, %d) rfcnt=%d\n",
		    (u_long)qp, qp->s_rfd, qp->s_refcnt));

	if (qp->s_bufsize != 0)
		free(qp->s_buf);
	FD_CLR(qp->s_rfd, &mask);
	(void) my_close(qp->s_rfd);
	if (qp == streamq) {
		streamq = qp->s_next;
	} else {
		for (qsp = streamq;
		     qsp && (qsp->s_next != qp);
		     qsp = qsp->s_next)
			;
		if (qsp) {
			qsp->s_next = qp->s_next;
		}
	}
	free((char *)qp);
}

/* void
 * sqflush(allbut)
 *	call sqrm() on all open streams except `allbut'
 * side effects:
 *	global list `streamq' modified
 * idiocy:
 *	is N^2 due to the scan inside of sqrm()
 */
void
sqflush(allbut)
	register struct qstream *allbut;
{
	register struct qstream *sp, *spnext;

	for (sp = streamq; sp != NULL; sp = spnext) {
		spnext = sp->s_next;
		if (sp != allbut)
			sqrm(sp);
	}
}

/* void
 * dqflush(gen)
 *	close/deallocate all the udp sockets, unless `gen' != (time_t)0
 *	in which case all those not from this generation (except 0.0.0.0)
 *	will be deleted, and syslog() will be called.
 * known bugs:
 *	the above text is impenetrable.
 * side effects:
 *	global list `datagramq' is modified.
 */
void
dqflush(gen)
	register time_t gen;
{
	register struct qdatagram *this, *prev, *next;

	prev = NULL;
	for (this = datagramq; this != NULL; this = next) {
		next = this->dq_next;
		if (gen != (time_t)0) {
			if (this->dq_addr.s_addr == INADDR_ANY ||
			    this->dq_gen == gen) {
				prev = this;
				continue;
			}
			syslog(LOG_NOTICE, "interface [%s] missing; deleting",
			       inet_ntoa(this->dq_addr));
		}
		FD_CLR(this->dq_dfd, &mask);
		my_close(this->dq_dfd);
		free(this);
		if (prev == NULL)
			datagramq = next;
		else
			prev->dq_next = next;
	}
}

/* int
 * sq_here(sp)
 *	determine whether stream 'sp' is still on the streamq
 * return:
 *	boolean: is it here?
 */
static int
sq_here(sp)
	register struct qstream *sp;
{
	register struct qstream *t;

	for (t = streamq;  t != NULL;  t = t->s_next)
		if (t == sp)
			return (1);
	return (0);
}

/*
 * Initiate query on stream;
 * mark as referenced and stop selecting for input.
 */
static void
sq_query(sp)
	register struct qstream *sp;
{
	sp->s_refcnt++;
	FD_CLR(sp->s_rfd, &mask);
}

/*
 * Note that the current request on a stream has completed,
 * and that we should continue looking for requests on the stream.
 */
void
sq_done(sp)
	register struct qstream *sp;
{

	sp->s_refcnt = 0;
	sp->s_time = tt.tv_sec;
	FD_SET(sp->s_rfd, &mask);
}

void
ns_setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	register char *cp;
	struct sockaddr_in sin;
	char buf[80];

	cp = Argv[0];
	size = sizeof(sin);
	if (getpeername(s, (struct sockaddr *)&sin, &size) == 0)
		(void) sprintf(buf, "-%s [%s]", a, inet_ntoa(sin.sin_addr));
	else {
		syslog(LOG_DEBUG, "getpeername: %m");
		(void) sprintf(buf, "-%s", a);
	}
	(void) strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}

u_int32_t
net_mask(in)
	struct in_addr in;
{
	register u_int32_t i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	else if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	else
		return (htonl(IN_CLASSC_NET));
}

/*
 * These are here in case we ever want to get more clever, like perhaps
 * using a bitmap to keep track of outstanding queries and a random
 * allocation scheme to make it a little harder to predict them.  Note
 * that the resolver will need the same protection so the cleverness
 * should be put there rather than here; this is just an interface layer.
 */

void
nsid_init()
{
	nsid_state = res_randomid();
}

u_int16_t
nsid_next()
{
	if (nsid_state == 65535)
		nsid_state = 0;
	else
		nsid_state++;
	return (nsid_state);
}

#if defined(BSD43_BSD43_NFS)
/* junk needed for old Sun NFS licensees */
#undef dn_skipname
extern char *dn_skipname();
char *(*hack_skipname)() = dn_skipname;
#endif
