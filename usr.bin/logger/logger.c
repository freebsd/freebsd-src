/*
 * Copyright (c) 1983, 1993
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)logger.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/logger/logger.c,v 1.5.2.1 2000/08/08 03:11:22 ps Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SYSLOG_NAMES
#include <syslog.h>

int	decode __P((char *, CODE *));
int	pencode __P((char *));
static void	logmessage __P((int, char *, char *));
static void	usage __P((void));

/*
 * logger -- read and log utility
 *
 *	Reads from an input and arranges to write the result on the system
 *	log.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, logflags, pri;
	char *tag, *host, buf[1024];

	tag = NULL;
	host = NULL;
	pri = LOG_NOTICE;
	logflags = 0;
	unsetenv("TZ");
	while ((ch = getopt(argc, argv, "f:h:ip:st:")) != -1)
		switch((char)ch) {
		case 'f':		/* file to log */
			if (freopen(optarg, "r", stdin) == NULL)
				err(1, "%s", optarg);
			break;
		case 'h':		/* hostname to deliver to */
			host = optarg;
			break;
		case 'i':		/* log process id also */
			logflags |= LOG_PID;
			break;
		case 'p':		/* priority */
			pri = pencode(optarg);
			break;
		case 's':		/* log to standard error */
			logflags |= LOG_PERROR;
			break;
		case 't':		/* tag */
			tag = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* setup for logging */
	openlog(tag ? tag : getlogin(), logflags, 0);
	(void) fclose(stdout);

	/* log input line if appropriate */
	if (argc > 0) {
		register char *p, *endp;
		int len;

		for (p = buf, endp = buf + sizeof(buf) - 2; *argv;) {
			len = strlen(*argv);
			if (p + len > endp && p > buf) {
				logmessage(pri, host, buf);
				p = buf;
			}
			if (len > sizeof(buf) - 1)
				logmessage(pri, host, *argv++);
			else {
				if (p != buf)
					*p++ = ' ';
				bcopy(*argv++, p, len);
				*(p += len) = '\0';
			}
		}
		if (p != buf)
			logmessage(pri, host, buf);
	} else
		while (fgets(buf, sizeof(buf), stdin) != NULL)
			logmessage(pri, host, buf);
	exit(0);
}

/*
 *  Send the message to syslog, either on the local host, or on a remote host
 */
void 
logmessage(int pri, char *host, char *buf)
{
	static int sock = -1;
	static struct sockaddr_in sin;
	char *line;
	int len;

	if (host == NULL) {
		syslog(pri, "%s", buf);
		return;
	}

	if (sock == -1) {	/* set up socket stuff */
		struct servent *sp;
		struct hostent *hp = NULL;

		sin.sin_family = AF_INET;

		if ((sp = getservbyname("syslog", "udp")) == NULL)
			warnx ("syslog/udp: unknown service");	/* not fatal */
		sin.sin_port = (sp == NULL ? htons(514) : sp->s_port);

		/* resolve hostname */
		if (!(inet_aton(host, &sin.sin_addr)) &&
		    (hp = gethostbyname(host)) == NULL)
			errx(1, "unknown host: %s", host);
		if (hp != NULL)
			memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));

		sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock < 0)
			errx(1, "socket");
	}

	if ((len = asprintf(&line, "<%d>%s", pri, buf)) == -1)
		errx(1, "asprintf");

	if (sendto(sock, line, len, 0, (struct sockaddr *)&sin, sizeof(sin))
	    < len)
		warnx ("sendmsg");

	free(line);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
pencode(s)
	register char *s;
{
	char *save;
	int fac, lev;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			errx(1, "unknown facility name: %s", save);
		*s++ = '.';
	}
	else {
		fac = 0;
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		errx(1, "unknown priority name: %s", save);
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int
decode(name, codetab)
	char *name;
	CODE *codetab;
{
	register CODE *c;

	if (isdigit(*name))
		return (atoi(name));

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: %s\n",
	    "logger [-is] [-f file] [-h host] [-p pri] [-t tag] [message ...]"
	    );
	exit(1);
}
