/*	$KAME$	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/faithd/tcp.c,v 1.1.2.1 2000/07/15 07:36:22 kris Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "faithd.h"

static char tcpbuf[16*1024];
	/* bigger than MSS and may be lesser than window size */
static int tblen, tboff, oob_exists;
static fd_set readfds, writefds, exceptfds;
static char atmark_buf[2];
static pid_t cpid = (pid_t)0;
static pid_t ppid = (pid_t)0;
static time_t child_lastactive = (time_t)0;
static time_t parent_lastactive = (time_t)0;

static void sig_ctimeout __P((int));
static void sig_child __P((int));
static void notify_inactive __P((void));
static void notify_active __P((void));
static void send_data __P((int, int, const char *, int));
static void relay __P((int, int, const char *, int));

/*
 * Inactivity timer:
 * - child side (ppid != 0) will send SIGUSR1 to parent every (FAITH_TIMEOUT/4)
 *   second if traffic is active.  if traffic is inactive, don't send SIGUSR1.
 * - parent side (ppid == 0) will check the last SIGUSR1 it have seen.
 */
static void
sig_ctimeout(int sig)
{
	/* parent side: record notification from the child */
	if (dflag)
		syslog(LOG_DEBUG, "activity timer from child");
	child_lastactive = time(NULL);
}

/* parent will terminate if child dies. */
static void
sig_child(int sig)
{
	int status;
	pid_t pid;

	pid = wait3(&status, WNOHANG, (struct rusage *)0);
	if (pid && status)
		syslog(LOG_WARNING, "child %d exit status 0x%x", pid, status);
	exit_failure("terminate connection due to child termination");
}

static void
notify_inactive()
{
	time_t t;

	/* only on parent side... */
	if (ppid)
		return;

	/* parent side should check for timeout. */
	t = time(NULL);
	if (dflag) {
		syslog(LOG_DEBUG, "parent side %sactive, child side %sactive",
			(FAITH_TIMEOUT < t - parent_lastactive) ? "in" : "",
			(FAITH_TIMEOUT < t - child_lastactive) ? "in" : "");
	}

	if (FAITH_TIMEOUT < t - child_lastactive
	 && FAITH_TIMEOUT < t - parent_lastactive) {
		/* both side timeouted */
		signal(SIGCHLD, SIG_DFL);
		kill(cpid, SIGTERM);
		wait(NULL);
		exit_failure("connection timeout");
		/* NOTREACHED */
	}
}

static void
notify_active()
{
	if (ppid) {
		/* child side: notify parent of active traffic */
		time_t t;
		t = time(NULL);
		if (FAITH_TIMEOUT / 4 < t - child_lastactive) {
			if (kill(ppid, SIGUSR1) < 0) {
				exit_failure("terminate connection due to parent termination");
				/* NOTREACHED */
			}
			child_lastactive = t;
		}
	} else {
		/* parent side */
		parent_lastactive = time(NULL);
	}
}

static void
send_data(int s_rcv, int s_snd, const char *service, int direction)
{
	int cc;

	if (oob_exists) {
		cc = send(s_snd, atmark_buf, 1, MSG_OOB);
		if (cc == -1)
			goto retry_or_err;
		oob_exists = 0;
		FD_SET(s_rcv, &exceptfds);
	}

	for (; tboff < tblen; tboff += cc) {
		cc = write(s_snd, tcpbuf + tboff, tblen - tboff);
		if (cc < 0)
			goto retry_or_err;
	}
#ifdef DEBUG
	if (tblen) {
		if (tblen >= sizeof(tcpbuf))
			tblen = sizeof(tcpbuf) - 1;
	    	tcpbuf[tblen] = '\0';
		syslog(LOG_DEBUG, "from %s (%dbytes): %s",
		       direction == 1 ? "client" : "server", tblen, tcpbuf);
	}
#endif /* DEBUG */
	tblen = 0; tboff = 0;
	FD_CLR(s_snd, &writefds);
	FD_SET(s_rcv, &readfds);
	return;
    retry_or_err:
	if (errno != EAGAIN)
		exit_failure("writing relay data failed: %s", ERRSTR);
	FD_SET(s_snd, &writefds);
}

static void
relay(int s_rcv, int s_snd, const char *service, int direction)
{
	int atmark, error, maxfd;
	struct timeval tv;
	fd_set oreadfds, owritefds, oexceptfds;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	fcntl(s_snd, F_SETFD, O_NONBLOCK);
	oreadfds = readfds; owritefds = writefds; oexceptfds = exceptfds;
	FD_SET(s_rcv, &readfds); FD_SET(s_rcv, &exceptfds);
	oob_exists = 0;
	maxfd = (s_rcv > s_snd) ? s_rcv : s_snd;

	for (;;) {
		tv.tv_sec = FAITH_TIMEOUT / 4;
		tv.tv_usec = 0;
		oreadfds = readfds;
		owritefds = writefds;
		oexceptfds = exceptfds;
		error = select(maxfd + 1, &readfds, &writefds, &exceptfds, &tv);
		if (error == -1) {
			if (errno == EINTR)
				continue;
			exit_failure("select: %s", ERRSTR);
		} else if (error == 0) {
			readfds = oreadfds;
			writefds = owritefds;
			exceptfds = oexceptfds;
			notify_inactive();
			continue;
		}

		/* activity notification */
		notify_active();

		if (FD_ISSET(s_rcv, &exceptfds)) {
			error = ioctl(s_rcv, SIOCATMARK, &atmark);
			if (error != -1 && atmark == 1) {
				int cc;
			    oob_read_retry:
				cc = read(s_rcv, atmark_buf, 1);
				if (cc == 1) {
					FD_CLR(s_rcv, &exceptfds);
					FD_SET(s_snd, &writefds);
					oob_exists = 1;
				} else if (cc == -1) {
					if (errno == EINTR)
						goto oob_read_retry;
					exit_failure("reading oob data failed"
						     ": %s",
						     ERRSTR);
				}
			}
		}
		if (FD_ISSET(s_rcv, &readfds)) {
		    relaydata_read_retry:
			tblen = read(s_rcv, tcpbuf, sizeof(tcpbuf));
			tboff = 0;

			switch (tblen) {
			case -1:
				if (errno == EINTR)
					goto relaydata_read_retry;
				exit_failure("reading relay data failed: %s",
					     ERRSTR);
				/* NOTREACHED */
			case 0:
				/* to close opposite-direction relay process */
				shutdown(s_snd, 0);

				close(s_rcv);
				close(s_snd);
				exit_success("terminating %s relay", service);
				/* NOTREACHED */
			default:
				FD_CLR(s_rcv, &readfds);
				FD_SET(s_snd, &writefds);
				break;
			}
		}
		if (FD_ISSET(s_snd, &writefds))
			send_data(s_rcv, s_snd, service, direction);
	}
}

void
tcp_relay(int s_src, int s_dst, const char *service)
{
	syslog(LOG_INFO, "starting %s relay", service);

	child_lastactive = parent_lastactive = time(NULL);

	cpid = fork();
	switch (cpid) {
	case -1:
		exit_failure("tcp_relay: can't fork grand child: %s", ERRSTR);
		/* NOTREACHED */
	case 0:
		/* child process: relay going traffic */
		ppid = getppid();
		/* this is child so reopen log */
		closelog();
		openlog(logname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
		relay(s_src, s_dst, service, 1);
		/* NOTREACHED */
	default:
		/* parent process: relay coming traffic */
		ppid = (pid_t)0;
		signal(SIGUSR1, sig_ctimeout);
		signal(SIGCHLD, sig_child);
		relay(s_dst, s_src, service, 0);
		/* NOTREACHED */
	}
}
