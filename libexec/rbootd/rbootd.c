/*
 * Copyright (c) 1988, 1992 The University of Utah and the Center
 *	for Software Science (CSS).
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Center for Software Science of the University of Utah Computer
 * Science Department.  CSS requests users of this software to return
 * to css-dist@cs.utah.edu any improvements that they make and grant
 * CSS redistribution rights.
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
 *
 *	@(#)rbootd.c	8.2 (Berkeley) 2/22/94
 *	$Id: rbootd.c,v 1.3 1996/09/22 21:54:32 wosch Exp $
 *
 * Utah $Hdr: rbootd.c 3.1 92/07/06$
 * Author: Jeff Forys, University of Utah CSS
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rbootd.c	8.2 (Berkeley) 2/22/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "defs.h"


/* fd mask macros (backward compatibility with 4.2BSD) */
#ifndef	FD_SET
#ifdef	notdef
typedef	struct fd_set {		/* this should already be in 4.2 */
	int fds_bits[1];
} fd_set;
#endif
#define	FD_ZERO(p)	((p)->fds_bits[0] = 0)
#define	FD_SET(n, p)	((p)->fds_bits[0] |= (1 << (n)))
#define	FD_CLR(n, p)	((p)->fds_bits[0] &= ~(1 << (n)))
#define	FD_ISSET(n, p)	((p)->fds_bits[0] & (1 << (n)))
#endif

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c, fd, omask, maxfds;
	fd_set rset;

	/*
	 *  Find what name we are running under.
	 */
	ProgName = (ProgName = rindex(argv[0],'/')) ? ++ProgName : *argv;

	/*
	 *  Close any open file descriptors.
	 *  Temporarily leave stdin & stdout open for `-d',
	 *  and stderr open for any pre-syslog error messages.
	 */
	{
		int i, nfds = getdtablesize();

		for (i = 0; i < nfds; i++)
			if (i != fileno(stdin) && i != fileno(stdout) &&
			    i != fileno(stderr))
				(void) close(i);
	}

	/*
	 *  Parse any arguments.
	 */
	while ((c = getopt(argc, argv, "adi:")) !=  -1)
		switch(c) {
		    case 'a':
			BootAny++;
			break;
		    case 'd':
			DebugFlg++;
			break;
		    case 'i':
			IntfName = optarg;
			break;
		}
	for (; optind < argc; optind++) {
		if (ConfigFile == NULL)
			ConfigFile = argv[optind];
		else {
			fprintf(stderr,
			        "%s: too many config files (`%s' ignored)\n",
			        ProgName, argv[optind]);
		}
	}

	if (ConfigFile == NULL)			/* use default config file */
		ConfigFile = DfltConfig;

	if (DebugFlg) {
		DbgFp = stdout;				/* output to stdout */

		(void) signal(SIGUSR1, SIG_IGN);	/* dont muck w/DbgFp */
		(void) signal(SIGUSR2, SIG_IGN);
	} else {
		(void) fclose(stdin);			/* dont need these */
		(void) fclose(stdout);

		/*
		 *  Fork off a child to do the work & exit.
		 */
		switch(fork()) {
			case -1:	/* fork failed */
				fprintf(stderr, "%s: ", ProgName);
				perror("fork");
				Exit(0);
			case 0:		/* this is the CHILD */
				break;
			default:	/* this is the PARENT */
				_exit(0);
		}

		/*
		 *  Try to disassociate from the current tty.
		 */
		{
			char *devtty = "/dev/tty";
			int i;

			if ((i = open(devtty, O_RDWR)) < 0) {
				/* probably already disassociated */
				if (setpgrp(0, 0) < 0) {
					fprintf(stderr, "%s: ", ProgName);
					perror("setpgrp");
				}
			} else {
				if (ioctl(i, (u_long)TIOCNOTTY, (char *)0) < 0){
					fprintf(stderr, "%s: ", ProgName);
					perror("ioctl");
				}
				(void) close(i);
			}
		}

		(void) signal(SIGUSR1, DebugOn);
		(void) signal(SIGUSR2, DebugOff);
	}

	(void) fclose(stderr);		/* finished with it */

#ifdef SYSLOG4_2
	openlog(ProgName, LOG_PID);
#else
	openlog(ProgName, LOG_PID, LOG_DAEMON);
#endif

	/*
	 *  If no interface was specified, get one now.
	 *
	 *  This is convoluted because we want to get the default interface
	 *  name for the syslog("restarted") message.  If BpfGetIntfName()
	 *  runs into an error, it will return a syslog-able error message
	 *  (in `errmsg') which will be displayed here.
	 */
	if (IntfName == NULL) {
		char *errmsg;

		if ((IntfName = BpfGetIntfName(&errmsg)) == NULL) {
			syslog(LOG_NOTICE, "restarted (??)");
			syslog(LOG_ERR, errmsg);
			Exit(0);
		}
	}

	syslog(LOG_NOTICE, "restarted (%s)", IntfName);

	(void) signal(SIGHUP, ReConfig);
	(void) signal(SIGINT, Exit);
	(void) signal(SIGTERM, Exit);

	/*
	 *  Grab our host name and pid.
	 */
	if (gethostname(MyHost, MAXHOSTNAMELEN) < 0) {
		syslog(LOG_ERR, "gethostname: %m");
		Exit(0);
	}
	MyHost[MAXHOSTNAMELEN] = '\0';

	MyPid = getpid();

	/*
	 *  Write proc's pid to a file.
	 */
	{
		FILE *fp;

		if ((fp = fopen(PidFile, "w")) != NULL) {
			(void) fprintf(fp, "%d\n", MyPid);
			(void) fclose(fp);
		} else {
			syslog(LOG_WARNING, "fopen: failed (%s)", PidFile);
		}
	}

	/*
	 *  All boot files are relative to the boot directory, we might
	 *  as well chdir() there to make life easier.
	 */
	if (chdir(BootDir) < 0) {
		syslog(LOG_ERR, "chdir: %m (%s)", BootDir);
		Exit(0);
	}

	/*
	 *  Initial configuration.
	 */
	omask = sigblock(sigmask(SIGHUP));	/* prevent reconfig's */
	if (GetBootFiles() == 0)		/* get list of boot files */
		Exit(0);
	if (ParseConfig() == 0)			/* parse config file */
		Exit(0);

	/*
	 *  Open and initialize a BPF device for the appropriate interface.
	 *  If an error is encountered, a message is displayed and Exit()
	 *  is called.
	 */
	fd = BpfOpen();

	(void) sigsetmask(omask);		/* allow reconfig's */

	/*
	 *  Main loop: receive a packet, determine where it came from,
	 *  and if we service this host, call routine to handle request.
	 */
	maxfds = fd + 1;
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	for (;;) {
		struct timeval timeout;
		fd_set r;
		int nsel;

		r = rset;

		if (RmpConns == NULL) {		/* timeout isnt necessary */
			nsel = select(maxfds, &r, (fd_set *)0, (fd_set *)0,
			              (struct timeval *)0);
		} else {
			timeout.tv_sec = RMP_TIMEOUT;
			timeout.tv_usec = 0;
			nsel = select(maxfds, &r, (fd_set *)0, (fd_set *)0,
			              &timeout);
		}

		if (nsel < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %m");
			Exit(0);
		} else if (nsel == 0) {		/* timeout */
			DoTimeout();			/* clear stale conns */
			continue;
		}

		if (FD_ISSET(fd, &r)) {
			RMPCONN rconn;
			CLIENT *client, *FindClient();
			int doread = 1;

			while (BpfRead(&rconn, doread)) {
				doread = 0;

				if (DbgFp != NULL)	/* display packet */
					DispPkt(&rconn,DIR_RCVD);

				omask = sigblock(sigmask(SIGHUP));

				/*
				 *  If we do not restrict service, set the
				 *  client to NULL (ProcessPacket() handles
				 *  this).  Otherwise, check that we can
				 *  service this host; if not, log a message
				 *  and ignore the packet.
				 */
				if (BootAny) {
					client = NULL;
				} else if ((client=FindClient(&rconn))==NULL) {
					syslog(LOG_INFO,
					       "%s: boot packet ignored",
					       EnetStr(&rconn));
					(void) sigsetmask(omask);
					continue;
				}

				ProcessPacket(&rconn,client);

				(void) sigsetmask(omask);
			}
		}
	}
}

/*
**  DoTimeout -- Free any connections that have timed out.
**
**	Parameters:
**		None.
**
**	Returns:
**		Nothing.
**
**	Side Effects:
**		- Timed out connections in `RmpConns' will be freed.
*/
void
DoTimeout()
{
	register RMPCONN *rtmp;
	struct timeval now;

	(void) gettimeofday(&now, (struct timezone *)0);

	/*
	 *  For each active connection, if RMP_TIMEOUT seconds have passed
	 *  since the last packet was sent, delete the connection.
	 */
	for (rtmp = RmpConns; rtmp != NULL; rtmp = rtmp->next)
		if ((rtmp->tstamp.tv_sec + RMP_TIMEOUT) < now.tv_sec) {
			syslog(LOG_WARNING, "%s: connection timed out (%u)",
			       EnetStr(rtmp), rtmp->rmp.r_type);
			RemoveConn(rtmp);
		}
}

/*
**  FindClient -- Find client associated with a packet.
**
**	Parameters:
**		rconn - the new packet.
**
**	Returns:
**		Pointer to client info if found, NULL otherwise.
**
**	Side Effects:
**		None.
**
**	Warnings:
**		- This routine must be called with SIGHUP blocked since
**		  a reconfigure can invalidate the information returned.
*/

CLIENT *
FindClient(rconn)
	register RMPCONN *rconn;
{
	register CLIENT *ctmp;

	for (ctmp = Clients; ctmp != NULL; ctmp = ctmp->next)
		if (bcmp((char *)&rconn->rmp.hp_hdr.saddr[0],
		         (char *)&ctmp->addr[0], RMP_ADDRLEN) == 0)
			break;

	return(ctmp);
}

/*
**  Exit -- Log an error message and exit.
**
**	Parameters:
**		sig - caught signal (or zero if not dying on a signal).
**
**	Returns:
**		Does not return.
**
**	Side Effects:
**		- This process ceases to exist.
*/
void
Exit(sig)
	int sig;
{
	if (sig > 0)
		syslog(LOG_ERR, "going down on signal %d", sig);
	else
		syslog(LOG_ERR, "going down with fatal error");
	BpfClose();
	exit(1);
}

/*
**  ReConfig -- Get new list of boot files and reread config files.
**
**	Parameters:
**		None.
**
**	Returns:
**		Nothing.
**
**	Side Effects:
**		- All active connections are dropped.
**		- List of boot-able files is changed.
**		- List of clients is changed.
**
**	Warnings:
**		- This routine must be called with SIGHUP blocked.
*/
void
ReConfig(signo)
	int signo;
{
	syslog(LOG_NOTICE, "reconfiguring boot server");

	FreeConns();

	if (GetBootFiles() == 0)
		Exit(0);

	if (ParseConfig() == 0)
		Exit(0);
}

/*
**  DebugOff -- Turn off debugging.
**
**	Parameters:
**		None.
**
**	Returns:
**		Nothing.
**
**	Side Effects:
**		- Debug file is closed.
*/
void
DebugOff(signo)
	int signo;
{
	if (DbgFp != NULL)
		(void) fclose(DbgFp);

	DbgFp = NULL;
}

/*
**  DebugOn -- Turn on debugging.
**
**	Parameters:
**		None.
**
**	Returns:
**		Nothing.
**
**	Side Effects:
**		- Debug file is opened/truncated if not already opened,
**		  otherwise do nothing.
*/
void
DebugOn(signo)
	int signo;
{
	if (DbgFp == NULL) {
		if ((DbgFp = fopen(DbgFile, "w")) == NULL)
			syslog(LOG_ERR, "can't open debug file (%s)", DbgFile);
	}
}
