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
/*
static char sccsid[] = "From: @(#)syslog.c	8.4 (Berkeley) 3/18/94";
*/
static const char rcsid[] =
  "$Id: syslog.c,v 1.9.2.2 1998/03/05 22:19:54 brian Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static int	LogFile = -1;		/* fd for log */
static int	connected;		/* have done connect */
static int	opened;			/* have done openlog() */
static int	LogStat = 0;		/* status bits, set by openlog() */
static const char *LogTag = NULL;	/* string to tag the entry with */
static int	LogFacility = LOG_USER;	/* default facility code */
static int	LogMask = 0xff;		/* mask of priorities to be logged */
extern char	*__progname;		/* Program name, from crt0. */

static void	disconnectlog __P((void)); /* disconnect from syslogd */
static void	connectlog __P((void));	/* (re)connect to syslogd */

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
static
int writehook(cookie, buf, len)
	void	*cookie;	/* really [struct bufcookie *] */
	char	*buf;		/* characters to copy */
	int	len;		/* length to copy */
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
#if __STDC__
syslog(int pri, const char *fmt, ...)
#else
syslog(pri, fmt, va_alist)
	int pri;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

void
vsyslog(pri, fmt, ap)
	int pri;
	register const char *fmt;
	va_list ap;
{
	register int cnt;
	register char ch, *p;
	time_t now;
	int fd, saved_errno;
	char *stdp, tbuf[2048], fmt_cpy[1024];
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

	/* Check priority against setlogmask values. */
	if (!(LOG_MASK(LOG_PRI(pri)) & LogMask))
		return;

	saved_errno = errno;

	/* Set default facility if none specified. */
	if ((pri & LOG_FACMASK) == 0)
		pri |= LogFacility;

	/* Create the primary stdio hook */
	tbuf_cookie.base = tbuf;
	tbuf_cookie.left = sizeof(tbuf);
	fp = fwopen(&tbuf_cookie, writehook);
	if (fp == NULL)
		return;

	/* Build the message. */
	(void)time(&now);
	(void)fprintf(fp, "<%d>", pri);
	(void)fprintf(fp, "%.15s ", ctime(&now) + 4);
	if (LogStat & LOG_PERROR) {
		/* Transfer to string buffer */
		(void)fflush(fp);
		stdp = tbuf + (sizeof(tbuf) - tbuf_cookie.left);
	}
	if (LogTag == NULL)
		LogTag = __progname;
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
			return;
		}

		/* Substitute error message for %m. */
		for ( ; (ch = *fmt); ++fmt)
			if (ch == '%' && fmt[1] == 'm') {
				++fmt;
				fputs(strerror(saved_errno), fmt_fp);
			} else
				fputc(ch, fmt_fp);

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

	/* Output to stderr if requested. */
	if (LogStat & LOG_PERROR) {
		struct iovec iov[2];
		register struct iovec *v = iov;

		v->iov_base = stdp;
		v->iov_len = cnt - (stdp - tbuf);
		++v;
		v->iov_base = "\n";
		v->iov_len = 1;
		(void)writev(STDERR_FILENO, iov, 2);
	}

	/* Get connected, output the message to the local logger. */
	if (!opened)
		openlog(LogTag, LogStat | LOG_NDELAY, 0);
	connectlog();
	if (send(LogFile, tbuf, cnt, 0) >= 0)
		return;

	/*
	 * If the send() failed, the odds are syslogd was restarted.
	 * Make one (only) attempt to reconnect to /dev/log.
	 */
	disconnectlog();
	connectlog();
	if (send(LogFile, tbuf, cnt, 0) >= 0)
		return;

	/*
	 * Output the message to the console; don't worry about blocking,
	 * if console blocks everything will.  Make sure the error reported
	 * is the one from the syslogd failure.
	 */
	if (LogStat & LOG_CONS &&
	    (fd = open(_PATH_CONSOLE, O_WRONLY, 0)) >= 0) {
		struct iovec iov[2];
		register struct iovec *v = iov;

		p = strchr(tbuf, '>') + 1;
		v->iov_base = p;
		v->iov_len = cnt - (p - tbuf);
		++v;
		v->iov_base = "\r\n";
		v->iov_len = 2;
		(void)writev(fd, iov, 2);
		(void)close(fd);
	}
}
static void
disconnectlog()
{
	/*
	 * If the user closed the FD and opened another in the same slot,
	 * that's their problem.  They should close it before calling on
	 * system services.
	 */
	if (LogFile != -1) {
		close(LogFile);
		LogFile = -1;
	}
	connected = 0;			/* retry connect */
}

static void
connectlog()
{
	struct sockaddr_un SyslogAddr;	/* AF_UNIX address of local logger */

	if (LogFile == -1) {
		if ((LogFile = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
			return;
		(void)fcntl(LogFile, F_SETFD, 1);
	}
	if (LogFile != -1 && !connected) {
		SyslogAddr.sun_len = sizeof(SyslogAddr);
		SyslogAddr.sun_family = AF_UNIX;
		(void)strncpy(SyslogAddr.sun_path, _PATH_LOG,
		    sizeof SyslogAddr.sun_path - 1);
		SyslogAddr.sun_path[sizeof SyslogAddr.sun_path - 1] = '\0';
		connected = connect(LogFile, (struct sockaddr *)&SyslogAddr,
			sizeof(SyslogAddr)) != -1;

		if (!connected) {
			/*
			 * Try the old "/dev/log" path, for backward
			 * compatibility.
			 */
			(void)strncpy(SyslogAddr.sun_path, _PATH_OLDLOG,
			    sizeof SyslogAddr.sun_path - 1);
			connected = connect(LogFile,
				(struct sockaddr *)&SyslogAddr,
				sizeof(SyslogAddr)) != -1;
		}

		if (!connected) {
			(void)close(LogFile);
			LogFile = -1;
		}
	}
}

void
openlog(ident, logstat, logfac)
	const char *ident;
	int logstat, logfac;
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
closelog()
{
	(void)close(LogFile);
	LogFile = -1;
	connected = 0;
}

/* setlogmask -- set the log mask level */
int
setlogmask(pmask)
	int pmask;
{
	int omask;

	omask = LogMask;
	if (pmask != 0)
		LogMask = pmask;
	return (omask);
}
