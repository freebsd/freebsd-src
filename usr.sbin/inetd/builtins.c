/*-
 * Copyright (c) 1983, 1991, 1993, 1994
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: builtins.c,v 1.8 1999/07/24 12:35:50 sheldonh Exp $
 *
 */

#include <sys/filio.h>
#include <sys/ioccom.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "inetd.h"

extern int	 debug;
extern struct servtab *servtab;

char ring[128];
char *endring;

int check_loop __P((struct sockaddr_in *, struct servtab *sep));
void inetd_setproctitle __P((char *, int));

struct biltin biltins[] = {
	/* Echo received data */
	{ "echo",	SOCK_STREAM,	1, -1,	echo_stream },
	{ "echo",	SOCK_DGRAM,	0, 1,	echo_dg },

	/* Internet /dev/null */
	{ "discard",	SOCK_STREAM,	1, -1,	discard_stream },
	{ "discard",	SOCK_DGRAM,	0, 1,	discard_dg },

	/* Return 32 bit time since 1970 */
	{ "time",	SOCK_STREAM,	0, -1,	machtime_stream },
	{ "time",	SOCK_DGRAM,	0, 1,	machtime_dg },

	/* Return human-readable time */
	{ "daytime",	SOCK_STREAM,	0, -1,	daytime_stream },
	{ "daytime",	SOCK_DGRAM,	0, 1,	daytime_dg },

	/* Familiar character generator */
	{ "chargen",	SOCK_STREAM,	1, -1,	chargen_stream },
	{ "chargen",	SOCK_DGRAM,	0, 1,	chargen_dg },

	{ "tcpmux",	SOCK_STREAM,	1, -1,	(void (*)())tcpmux },

	{ "auth",	SOCK_STREAM,	1, -1,	ident_stream },

	{ NULL }
};

/*
 * RFC864 Character Generator Protocol. Generates character data without
 * any regard for input.
 */

void
initring()
{
	int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
void
chargen_dg(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	struct sockaddr_in sin;
	static char *rs;
	int len, size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sin);
	if (recvfrom(s, text, sizeof(text), 0,
		     (struct sockaddr *)&sin, &size) < 0)
		return;

	if (check_loop(&sin, sep))
		return;

	if ((len = endring - rs) >= LINESIZ)
		memmove(text, rs, LINESIZ);
	else {
		memmove(text, rs, len);
		memmove(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0,
		      (struct sockaddr *)&sin, sizeof(sin));
}

/* ARGSUSED */
void
chargen_stream(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	int len;
	char *rs, text[LINESIZ+2];

	inetd_setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memmove(text, rs, LINESIZ);
		else {
			memmove(text, rs, len);
			memmove(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/*
 * RFC867 Daytime Protocol. Sends the current date and time as an ascii
 * character string without any regard for input.
 */

/* ARGSUSED */
void
daytime_dg(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;
	struct sockaddr_in sin;
	int size;

	clock = time((time_t *) 0);

	size = sizeof(sin);
	if (recvfrom(s, buffer, sizeof(buffer), 0,
		     (struct sockaddr *)&sin, &size) < 0)
		return;

	if (check_loop(&sin, sep))
		return;

	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, strlen(buffer), 0,
		      (struct sockaddr *)&sin, sizeof(sin));
}

/* ARGSUSED */
void
daytime_stream(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;

	clock = time((time_t *) 0);

	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, strlen(buffer));
}

/*
 * RFC863 Discard Protocol. Any data received is thrown away and no response
 * is sent.
 */

/* ARGSUSED */
void
discard_dg(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

/* ARGSUSED */
void
discard_stream(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	int ret;
	char buffer[BUFSIZE];

	inetd_setproctitle(sep->se_service, s);
	while (1) {
		while ((ret = read(s, buffer, sizeof(buffer))) > 0)
			;
		if (ret == 0 || errno != EINTR)
			break;
	}
	exit(0);
}

/*
 * RFC862 Echo Protocol. Any data received is sent back to the sender as
 * received.
 */

/* ARGSUSED */
void
echo_dg(s, sep)			/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i, size;
	struct sockaddr_in sin;

	size = sizeof(sin);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0,
			  (struct sockaddr *)&sin, &size)) < 0)
		return;

	if (check_loop(&sin, sep))
		return;

	(void) sendto(s, buffer, i, 0, (struct sockaddr *)&sin,
		      sizeof(sin));
}

/* ARGSUSED */
void
echo_stream(s, sep)		/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i;

	inetd_setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
	exit(0);
}

/*
 * RFC1413 Identification Protocol. Given a TCP port number pair, return a
 * character string which identifies the owner of that connection on the
 * server's system. Extended to allow for ~/.fakeid support.
 */

/* ARGSUSED */
void
iderror(lport, fport, s, er)
	int lport, fport, s, er;
{
	char *p;

	asprintf(&p, "%d , %d : ERROR : %s\r\n", lport, fport, 
	    er == -1 ? "HIDDEN-USER" : er ? strerror(er) : "UNKNOWN-ERROR");
	if (p == NULL) {
		syslog(LOG_ERR, "Out of memory.");
		exit(EX_OSERR);
	}
	write(s, p, strlen(p));
	free(p);

	exit(0);
}

/* ARGSUSED */
void
ident_stream(s, sep)		/* Ident service */
	int s;
	struct servtab *sep;
{
	struct sockaddr_in sin[2];
	struct ucred uc;
	struct passwd *pw;
	struct timeval tv = {
		10,
		0
	};
	fd_set fdset;
	char buf[BUFSIZE], *cp = NULL, *p, **av, *osname = NULL;
	int len, c, rflag = 0, fflag = 0, argc = 0;
	u_short lport, fport;

	inetd_setproctitle(sep->se_service, s);
	optind = 1;
	optreset = 1;
	for (av = sep->se_argv; *av; av++)
		argc++;
	if (argc) {
		while ((c = getopt(argc, sep->se_argv, "fro:t:")) != -1)
			switch (c) {
			case 'f':
				fflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			case 'o':
				osname = optarg;
				break;
			case 't':
			{
				int sec, usec;

				switch (sscanf(optarg, "%d.%d", &sec, &usec)) {
				case 2:
					tv.tv_usec = usec;
				case 1:
					tv.tv_sec = sec;
					break;
				default:
					if (debug)
						warnx("bad -t argument");
					break;
				}
			}
			default:
				break;
			}
	}
	if (osname == NULL) {
		struct utsname un;

		if (uname(&un) == -1)
			iderror(0, 0, s, errno);
		osname = un.sysname;
	}
	len = sizeof(sin[0]);
	if (getsockname(s, (struct sockaddr *)&sin[0], &len) == -1)
		iderror(0, 0, s, errno);
	len = sizeof(sin[1]);
	if (getpeername(s, (struct sockaddr *)&sin[1], &len) == -1)
		iderror(0, 0, s, errno);
	FD_ZERO(&fdset);
	FD_SET(s, &fdset);
	if (select(s + 1, &fdset, NULL, NULL, &tv) == -1)
		iderror(0, 0, s, errno);
	if (ioctl(s, FIONREAD, &len) == -1)
		iderror(0, 0, s, errno);
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	len = read(s, buf, len);
	if (len == -1)
		iderror(0, 0, s, errno);
	buf[len] = '\0';
	if (sscanf(buf, "%hu , %hu", &lport, &fport) != 2)
		iderror(0, 0, s, 0);
	if (!rflag)
		iderror(lport, fport, s, -1);
	sin[0].sin_port = htons(lport);
	sin[1].sin_port = htons(fport);
	len = sizeof(uc);
	if (sysctlbyname("net.inet.tcp.getcred", &uc, &len, sin,
	    sizeof(sin)) == -1)
		iderror(lport, fport, s, errno);
	pw = getpwuid(uc.cr_uid);
	if (pw == NULL)
		iderror(lport, fport, s, errno);
	if (fflag) {
		FILE *fakeid = NULL;
		char fakeid_path[PATH_MAX];
		struct stat sb;

		seteuid(pw->pw_uid);
		setegid(pw->pw_gid);
		snprintf(fakeid_path, sizeof(fakeid_path), "%s/.fakeid",
		    pw->pw_dir);
		if ((fakeid = fopen(fakeid_path, "r")) != NULL &&
		    fstat(fileno(fakeid), &sb) != -1 && S_ISREG(sb.st_mode)) {
			buf[sizeof(buf) - 1] = '\0';
			if (fgets(buf, sizeof(buf), fakeid) == NULL) {
				cp = pw->pw_name;
				fclose(fakeid);
				goto printit;
			}
			fclose(fakeid);
			strtok(buf, "\r\n");
			if (strlen(buf) > 16)
				buf[16] = '\0';
			cp = buf;
			while (isspace(*cp))
				cp++;
			strtok(cp, " \t");
			if (!*cp || getpwnam(cp))
				cp = getpwuid(uc.cr_uid)->pw_name;
		} else
			cp = pw->pw_name;
	} else
		cp = pw->pw_name;
printit:
	if (asprintf(&p, "%d , %d : USERID : %s : %s\r\n", lport, fport, osname,
	    cp) == -1) {
		syslog(LOG_ERR, "Out of memory.");
		exit(EX_OSERR);
	}
	write(s, p, strlen(p));
	free(p);
	
	exit(0);
}

/*
 * RFC738 Time Server.
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

unsigned long
machtime()
{
	struct timeval tv;

	if (gettimeofday(&tv, (struct timezone *)NULL) < 0) {
		if (debug)
			warnx("unable to get time of day");
		return (0L);
	}
#define	OFFSET ((u_long)25567 * 24*60*60)
	return (htonl((long)(tv.tv_sec + OFFSET)));
#undef OFFSET
}

/* ARGSUSED */
void
machtime_dg(s, sep)
	int s;
	struct servtab *sep;
{
	unsigned long result;
	struct sockaddr_in sin;
	int size;

	size = sizeof(sin);
	if (recvfrom(s, (char *)&result, sizeof(result), 0,
		     (struct sockaddr *)&sin, &size) < 0)
		return;

	if (check_loop(&sin, sep))
		return;

	result = machtime();
	(void) sendto(s, (char *) &result, sizeof(result), 0,
		      (struct sockaddr *)&sin, sizeof(sin));
}

/* ARGSUSED */
void
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	unsigned long result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/*
 * RFC1078 TCP Port Service Multiplexer (TCPMUX). Service connections to
 * services based on the service name sent.
 *
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */

#define MAX_SERV_LEN	(256+2)		/* 2 bytes for \r\n */
#define strwrite(fd, buf)	(void) write(fd, buf, sizeof(buf)-1)

static int		/* # of characters upto \r,\n or \0 */
getline(fd, buf, len)
	int fd;
	char *buf;
	int len;
{
	int count = 0, n;
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_DFL;
	sigaction(SIGALRM, &sa, (struct sigaction *)0);
	do {
		alarm(10);
		n = read(fd, buf, len-count);
		alarm(0);
		if (n == 0)
			return (count);
		if (n < 0)
			return (-1);
		while (--n >= 0) {
			if (*buf == '\r' || *buf == '\n' || *buf == '\0')
				return (count);
			count++;
			buf++;
		}
	} while (count < len);
	return (count);
}

struct servtab *
tcpmux(s)
	int s;
{
	struct servtab *sep;
	char service[MAX_SERV_LEN+1];
	int len;

	/* Get requested service name */
	if ((len = getline(s, service, MAX_SERV_LEN)) < 0) {
		strwrite(s, "-Error reading service name\r\n");
		return (NULL);
	}
	service[len] = '\0';

	if (debug)
		warnx("tcpmux: someone wants %s", service);

	/*
	 * Help is a required command, and lists available services,
	 * one per line.
	 */
	if (!strcasecmp(service, "help")) {
		for (sep = servtab; sep; sep = sep->se_next) {
			if (!ISMUX(sep))
				continue;
			(void)write(s,sep->se_service,strlen(sep->se_service));
			strwrite(s, "\r\n");
		}
		return (NULL);
	}

	/* Try matching a service in inetd.conf with the request */
	for (sep = servtab; sep; sep = sep->se_next) {
		if (!ISMUX(sep))
			continue;
		if (!strcasecmp(service, sep->se_service)) {
			if (ISMUXPLUS(sep)) {
				strwrite(s, "+Go\r\n");
			}
			return (sep);
		}
	}
	strwrite(s, "-Service not available\r\n");
	return (NULL);
}
