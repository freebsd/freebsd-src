/*
 * Copyright (c) 1983, 1988, 1993
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)syslog.c	8.5 (Berkeley) 4/29/95";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <stdarg.h>
#include "un-namespace.h"

#include "libc_private.h"

static int	LogFile = -1;		/* fd for log */
static int	status;			/* connection status */
static int	opened;			/* have done openlog() */
static int	LogStat = 0;		/* status bits, set by openlog() */
static const char *LogTag = NULL;	/* string to tag the entry with */
static int	LogFacility = LOG_USER;	/* default facility code */
static int	LogMask = 0xff;		/* mask of priorities to be logged */
static pthread_mutex_t	syslog_mutex = PTHREAD_MUTEX_INITIALIZER;

#define	THREAD_LOCK()							\
	do { 								\
		if (__isthreaded) _pthread_mutex_lock(&syslog_mutex);	\
	} while(0)
#define	THREAD_UNLOCK()							\
	do {								\
		if (__isthreaded) _pthread_mutex_unlock(&syslog_mutex);	\
	} while(0)

static void	disconnectlog(void); /* disconnect from syslogd */
static void	connectlog(void);	/* (re)connect to syslogd */
static void	openlog_unlocked(const char *, int, int);

enum {
	NOCONN = 0,
	CONNDEF,
	CONNPRIV,
};

/*
 * Format of the magic cookie passed through the stdio hook
 */
struct bufcookie {
	char	*base;	/* start of buffer */
	int	left;
};

/*
 * stdio write hook for writing to a static string buffer
 * XXX: Maybe one day, dynamically allocate it so that the line length
 *      is `unlimited'.
 */
static int
writehook(void *cookie, const char *buf, int len)
{
	struct bufcookie *h;	/* private `handle' */

	h = (struct bufcookie *)cookie;
	if (len > h->left) {
		/* clip in case of wraparound */
		len = h->left;
	}
	if (len > 0) {
		(void)memcpy(h->base, buf, len); /* `write' it. */
		h->base += len;
		h->left -= len;
	}
	return 0;
}

/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
void
syslog(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

void
vsyslog(int pri, const char *fmt, va_list ap)
{
	int cnt;
	char ch, *p;
	time_t now;
	int fd, saved_errno;
	char *stdp, tbuf[2048], fmt_cpy[1024], timbuf[26], errstr[64];
	FILE *fp, *fmt_fp;
	struct bufcookie tbuf_cookie;
	struct bufcookie fmt_cookie;

#define	INTERNALLOG	LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID
	/* Check for invalid bits. */
	if (pri & ~(LOG_PRIMASK|LOG_FACMASK)) {
		syslog(INTERNALLOG,
		    "syslog: unknown facility/priority: %x", pri);
		pri &= LOG_PRIMASK|LOG_FACMASK;
	}

	THREAD_LOCK();

	/* Check priority against setlogmask values. */
	if (!(LOG_MASK(LOG_PRI(pri)) & LogMask)) {
		THREAD_UNLOCK();
		return;
	}

	saved_errno = errno;

	/* Set default facility if none specified. */
	if ((pri & LOG_FACMASK) == 0)
		pri |= LogFacility;

	/* Create the primary stdio hook */
	tbuf_cookie.base = tbuf;
	tbuf_cookie.left = sizeof(tbuf);
	fp = fwopen(&tbuf_cookie, writehook);
	if (fp == NULL) {
		THREAD_UNLOCK();
		return;
	}

	/* Build the message. */
	(void)time(&now);
	(void)fprintf(fp, "<%d>", pri);
	(void)fprintf(fp, "%.15s ", ctime_r(&now, timbuf) + 4);
	if (LogStat & LOG_PERROR) {
		/* Transfer to string buffer */
		(void)fflush(fp);
		stdp = tbuf + (sizeof(tbuf) - tbuf_cookie.left);
	}
	if (LogTag == NULL)
		LogTag = _getprogname();
	if (LogTag != NULL)
		(void)fprintf(fp, "%s", LogTag);
	if (LogStat & LOG_PID)
		(void)fprintf(fp, "[%d]", getpid());
	if (LogTag != NULL) {
		(void)fprintf(fp, ": ");
	}

	/* Check to see if we can skip expanding the %m */
	if (strstr(fmt, "%m")) {

		/* Create the second stdio hook */
		fmt_cookie.base = fmt_cpy;
		fmt_cookie.left = sizeof(fmt_cpy) - 1;
		fmt_fp = fwopen(&fmt_cookie, writehook);
		if (fmt_fp == NULL) {
			fclose(fp);
			THREAD_UNLOCK();
			return;
		}

		/*
		 * Substitute error message for %m.  Be careful not to
		 * molest an escaped percent "%%m".  We want to pass it
		 * on untouched as the format is later parsed by vfprintf.
		 */
		for ( ; (ch = *fmt); ++fmt) {
			if (ch == '%' && fmt[1] == 'm') {
				++fmt;
				strerror_r(saved_errno, errstr, sizeof(errstr));
				fputs(errstr, fmt_fp);
			} else if (ch == '%' && fmt[1] == '%') {
				++fmt;
				fputc(ch, fmt_fp);
				fputc(ch, fmt_fp);
			} else {
				fputc(ch, fmt_fp);
			}
		}

		/* Null terminate if room */
		fputc(0, fmt_fp);
		fclose(fmt_fp);

		/* Guarantee null termination */
		fmt_cpy[sizeof(fmt_cpy) - 1] = '\0';

		fmt = fmt_cpy;
	}

	(void)vfprintf(fp, fmt, ap);
	(void)fclose(fp);

	cnt = sizeof(tbuf) - tbuf_cookie.left;

	/* Remove a trailing newline */
	if (tbuf[cnt - 1] == '\n')
		cnt--;

	/* Output to stderr if requested. */
	if (LogStat & LOG_PERROR) {
		struct iovec iov[2];
		struct iovec *v = iov;

		v->iov_base = stdp;
		v->iov_len = cnt - (stdp - tbuf);
		++v;
		v->iov_base = "\n";
		v->iov_len = 1;
		(void)_writev(STDERR_FILENO, iov, 2);
	}

	/* Get connected, output the message to the local logger. */
	if (!opened)
		openlog_unlocked(LogTag, LogStat | LOG_NDELAY, 0);
	connectlog();

	/*
	 * If the send() failed, there are two likely scenarios: 
	 *  1) syslogd was restarted
	 *  2) /var/run/log is out of socket buffer space, which
	 *     in most cases means local DoS.
	 * We attempt to reconnect to /var/run/log to take care of
	 * case #1 and keep send()ing data to cover case #2
	 * to give syslogd a chance to empty its socket buffer.
	 *
	 * If we are working with a priveleged socket, then take
	 * only one attempt, because we don't want to freeze a
	 * critical application like su(1) or sshd(8).
	 *
	 */

	if (send(LogFile, tbuf, cnt, 0) < 0) {
		if (errno != ENOBUFS) {
			disconnectlog();
			connectlog();
		}
		do {
			usleep(1);
			if (send(LogFile, tbuf, cnt, 0) >= 0) {
				THREAD_UNLOCK();
				return;
			}
			if (status == CONNPRIV)
				break;
		} while (errno == ENOBUFS);
	} else {
		THREAD_UNLOCK();
		return;
	}

	/*
	 * Output the message to the console; try not to block
	 * as a blocking console should not stop other processes.
	 * Make sure the error reported is the one from the syslogd failure.
	 */
	if (LogStat & LOG_CONS &&
	    (fd = _open(_PATH_CONSOLE, O_WRONLY|O_NONBLOCK, 0)) >= 0) {
		struct iovec iov[2];
		struct iovec *v = iov;

		p = strchr(tbuf, '>') + 1;
		v->iov_base = p;
		v->iov_len = cnt - (p - tbuf);
		++v;
		v->iov_base = "\r\n";
		v->iov_len = 2;
		(void)_writev(fd, iov, 2);
		(void)_close(fd);
	}

	THREAD_UNLOCK();
}

/* Should be called with mutex acquired */
static void
disconnectlog(void)
{
	/*
	 * If the user closed the FD and opened another in the same slot,
	 * that's their problem.  They should close it before calling on
	 * system services.
	 */
	if (LogFile != -1) {
		_close(LogFile);
		LogFile = -1;
	}
	status = NOCONN;			/* retry connect */
}

/* Should be called with mutex acquired */
static void
connectlog(void)
{
	struct sockaddr_un SyslogAddr;	/* AF_UNIX address of local logger */

	if (LogFile == -1) {
		if ((LogFile = _socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
			return;
		(void)_fcntl(LogFile, F_SETFD, 1);
	}
	if (LogFile != -1 && status == NOCONN) {
		SyslogAddr.sun_len = sizeof(SyslogAddr);
		SyslogAddr.sun_family = AF_UNIX;

		/*
		 * First try priveleged socket. If no success,
		 * then try default socket.
		 */
		(void)strncpy(SyslogAddr.sun_path, _PATH_LOG_PRIV,
		    sizeof SyslogAddr.sun_path);
		if (_connect(LogFile, (struct sockaddr *)&SyslogAddr,
		    sizeof(SyslogAddr)) != -1)
			status = CONNPRIV;

		if (status == NOCONN) {
			(void)strncpy(SyslogAddr.sun_path, _PATH_LOG,
			    sizeof SyslogAddr.sun_path);
			if (_connect(LogFile, (struct sockaddr *)&SyslogAddr,
			    sizeof(SyslogAddr)) != -1)
				status = CONNDEF;
		}

		if (status == NOCONN) {
			/*
			 * Try the old "/dev/log" path, for backward
			 * compatibility.
			 */
			(void)strncpy(SyslogAddr.sun_path, _PATH_OLDLOG,
			    sizeof SyslogAddr.sun_path);
			if (_connect(LogFile, (struct sockaddr *)&SyslogAddr,
			    sizeof(SyslogAddr)) != -1)
				status = CONNDEF;
		}

		if (status == NOCONN) {
			(void)_close(LogFile);
			LogFile = -1;
		}
	}
}

static void
openlog_unlocked(const char *ident, int logstat, int logfac)
{
	if (ident != NULL)
		LogTag = ident;
	LogStat = logstat;
	if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0)
		LogFacility = logfac;

	if (LogStat & LOG_NDELAY)	/* open immediately */
		connectlog();

	opened = 1;	/* ident and facility has been set */
}

void
openlog(const char *ident, int logstat, int logfac)
{
	THREAD_LOCK();
	openlog_unlocked(ident, logstat, logfac);
	THREAD_UNLOCK();
}


void
closelog(void)
{
	THREAD_LOCK();
	(void)_close(LogFile);
	LogFile = -1;
	LogTag = NULL;
	status = NOCONN;
	THREAD_UNLOCK();
}

/* setlogmask -- set the log mask level */
int
setlogmask(int pmask)
{
	int omask;

	THREAD_LOCK();
	omask = LogMask;
	if (pmask != 0)
		LogMask = pmask;
	THREAD_UNLOCK();
	return (omask);
}
