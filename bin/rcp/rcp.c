/*
 * Copyright (c) 1983, 1990 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rcp.c	5.32 (Berkeley) 2/25/91";
#endif /* not lint */

/*
 * rcp
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pathnames.h"

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
char	dst_realm_buf[REALM_SZ];
char	*dest_realm = NULL;
int	use_kerberos = 1;
CREDENTIALS 	cred;
Key_schedule	schedule;
extern	char	*krb_realmofhost();
#ifdef CRYPT
int	doencrypt = 0;
#define	OPTIONS	"dfk:prtx"
#else
#define	OPTIONS	"dfk:prt"
#endif
#else
#define	OPTIONS "dfprt"
#endif

struct passwd *pwd;
u_short	port;
uid_t	userid;
int errs, rem;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

typedef struct _buf {
	int	cnt;
	char	*buf;
} BUF;

void lostconn();

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	struct servent *sp;
	int ch, fflag, tflag;
	char *targ, *shell, *colon();

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != EOF)
		switch(ch) {
		/* user-visible flags */
		case 'p':			/* preserve access/mod times */
			++pflag;
			break;
		case 'r':
			++iamrecursive;
			break;
#ifdef	KERBEROS
		case 'k':
			strncpy(dst_realm_buf, optarg, REALM_SZ);
			dest_realm = dst_realm_buf;
			break;
#ifdef CRYPT
		case 'x':
			doencrypt = 1;
			/* des_set_key(cred.session, schedule); */
			break;
#endif
#endif
		/* rshd-invoked options (server) */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':			/* "to" */
			iamremote = 1;
			tflag = 1;
			break;

		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef KERBEROS
#ifdef CRYPT
	shell = doencrypt ? "ekshell" : "kshell";
#else
	shell = "kshell";
#endif
	sp = getservbyname(shell, "tcp");
	if (sp == NULL) {
		char	msgbuf[64];
		use_kerberos = 0;
		(void)snprintf(msgbuf, sizeof(msgbuf),
		    "can't get entry for %s/tcp service", shell);
		old_warning(msgbuf);
		sp = getservbyname(shell = "shell", "tcp");
	}
#else
	sp = getservbyname(shell = "shell", "tcp");
#endif
	if (sp == NULL) {
		(void)fprintf(stderr, "rcp: %s/tcp: unknown service\n", shell);
		exit(1);
	}
	port = sp->s_port;

	if (!(pwd = getpwuid(userid = getuid()))) {
		(void)fprintf(stderr, "rcp: unknown user %d.\n", (int)userid);
		exit(1);
	}

	if (fflag) {
		/* follow "protocol", send data */
		(void)response();
		(void)setuid(userid);
		source(argc, argv);
		exit(errs);
	}

	if (tflag) {
		/* receive data */
		(void)setuid(userid);
		sink(argc, argv);
		exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	/* command to be executed on remote system using "rsh" */
#ifdef	KERBEROS
	(void)snprintf(cmd, sizeof(cmd),
	    "rcp%s%s%s%s", iamrecursive ? " -r" : "",
#ifdef CRYPT
	    ((doencrypt && use_kerberos) ? " -x" : ""),
#else
	    "",
#endif
	    pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");
#else
	(void)snprintf(cmd, sizeof(cmd), "rcp%s%s%s",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "",
	    targetshouldbedirectory ? " -d" : "");
#endif

	(void)signal(SIGPIPE, lostconn);

	if (targ = colon(argv[argc - 1]))
		toremote(targ, argc, argv);	/* destination is remote host */
	else {
		tolocal(argc, argv);		/* destination is local host */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
}

toremote(targ, argc, argv)
	char *targ;
	int argc;
	char **argv;
{
	int i, len, tos;
	char *bp, *host, *src, *suser, *thost, *tuser;
	char *colon();

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if (thost = index(argv[argc - 1], '@')) {
		/* user@host */
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = index(argv[i], '@');
			len = strlen(_PATH_RSH) + strlen(argv[i]) +
			    strlen(src) + (tuser ? strlen(tuser) : 0) +
			    strlen(thost) + strlen(targ) + CMDNEEDS + 20;
			if (!(bp = malloc(len)))
				nospace();
			if (host) {
				*host++ = 0;
				suser = argv[i];
				if (*suser == '\0')
					suser = pwd->pw_name;
				else if (!okname(suser))
					continue;
				(void)snprintf(bp, len,
				    "%s %s -l %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, host, suser, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			} else
				(void)snprintf(bp, len,
				    "%s %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, argv[i], cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			(void)susystem(bp);
			(void)free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				len = strlen(targ) + CMDNEEDS + 20;
				if (!(bp = malloc(len)))
					nospace();
				(void)snprintf(bp, len, "%s -t %s", cmd, targ);
				host = thost;
#ifdef KERBEROS
				if (use_kerberos)
					rem = kerberos(&host, bp,
					    pwd->pw_name,
					    tuser ? tuser : pwd->pw_name);
				else
#endif
					rem = rcmd(&host, port, pwd->pw_name,
					    tuser ? tuser : pwd->pw_name,
					    bp, 0);
				if (rem < 0)
					exit(1);
				tos = IPTOS_THROUGHPUT;
				if (setsockopt(rem, IPPROTO_IP, IP_TOS,
				    (char *)&tos, sizeof(int)) < 0)
					perror("rcp: setsockopt TOS (ignored)");
				if (response() < 0)
					exit(1);
				(void)free(bp);
				(void)setuid(userid);
			}
			source(1, argv+i);
		}
	}
}

tolocal(argc, argv)
	int argc;
	char **argv;
{
	int i, len, tos;
	char *bp, *host, *src, *suser;
	char *colon();

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {	/* local to local */
			len = strlen(_PATH_CP) + strlen(argv[i]) +
			    strlen(argv[argc - 1]) + 20;
			if (!(bp = malloc(len)))
				nospace();
			(void)snprintf(bp, len, "%s%s%s %s %s", _PATH_CP,
			    iamrecursive ? " -r" : "", pflag ? " -p" : "",
			    argv[i], argv[argc - 1]);
			(void)susystem(bp);
			(void)free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		host = index(argv[i], '@');
		if (host) {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwd->pw_name;
			else if (!okname(suser))
				continue;
		} else {
			host = argv[i];
			suser = pwd->pw_name;
		}
		len = strlen(src) + CMDNEEDS + 20;
		if (!(bp = malloc(len)))
			nospace();
		(void)snprintf(bp, len, "%s -f %s", cmd, src);
#ifdef KERBEROS
		if (use_kerberos)
			rem = kerberos(&host, bp, pwd->pw_name, suser);
		else
#endif
			rem = rcmd(&host, port, pwd->pw_name, suser, bp, 0);
		(void)free(bp);
		if (rem < 0)
			continue;
		(void)seteuid(userid);
		tos = IPTOS_THROUGHPUT;
		if (setsockopt(rem, IPPROTO_IP, IP_TOS,
		    (char *)&tos, sizeof(int)) < 0)
			perror("rcp: setsockopt TOS (ignored)");
		sink(1, argv + argc - 1);
		(void)seteuid(0);
		(void)close(rem);
		rem = -1;
	}
}

verifydir(cp)
	char *cp;
{
	struct stat stb;

	if (stat(cp, &stb) >= 0) {
		if ((stb.st_mode & S_IFMT) == S_IFDIR)
			return;
		errno = ENOTDIR;
	}
	error("rcp: %s: %s.\n", cp, strerror(errno));
	exit(1);
}

char *
colon(cp)
	register char *cp;
{
	for (; *cp; ++cp) {
		if (*cp == ':')
			return(cp);
		if (*cp == '/')
			return(0);
	}
	return(0);
}

okname(cp0)
	char *cp0;
{
	register char *cp = cp0;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
	} while (*++cp);
	return(1);
bad:
	(void)fprintf(stderr, "rcp: invalid user name %s\n", cp0);
	return(0);
}

susystem(s)
	char *s;
{
	int status, pid, w;
	register sig_t istat, qstat;

	if ((pid = vfork()) == 0) {
		(void)setuid(userid);
		execl(_PATH_BSHELL, "sh", "-c", s, (char *)0);
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	(void)signal(SIGINT, istat);
	(void)signal(SIGQUIT, qstat);
	return(status);
}

source(argc, argv)
	int argc;
	char **argv;
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i;
	int x, readerr, f, amt;
	char *last, *name, buf[BUFSIZ];
	BUF *allocbuf();

	for (x = 0; x < argc; x++) {
		name = argv[x];
		if ((f = open(name, O_RDONLY, 0)) < 0) {
			error("rcp: %s: %s\n", name, strerror(errno));
			continue;
		}
		if (fstat(f, &stb) < 0)
			goto notreg;
		switch (stb.st_mode&S_IFMT) {

		case S_IFREG:
			break;

		case S_IFDIR:
			if (iamrecursive) {
				(void)close(f);
				rsource(name, &stb);
				continue;
			}
			/* FALLTHROUGH */
		default:
notreg:			(void)close(f);
			error("rcp: %s: not a plain file\n", name);
			continue;
		}
		last = rindex(name, '/');
		if (last == 0)
			last = name;
		else
			last++;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void)snprintf(buf, sizeof(buf),
			    "T%ld 0 %ld 0\n", stb.st_mtime, stb.st_atime);
			(void)write(rem, buf, (int)strlen(buf));
			if (response() < 0) {
				(void)close(f);
				continue;
			}
		}
		(void)snprintf(buf, sizeof(buf),
		    "C%04o %ld %s\n", stb.st_mode&07777, stb.st_size, last);
		(void)write(rem, buf, (int)strlen(buf));
		if (response() < 0) {
			(void)close(f);
			continue;
		}
		if ((bp = allocbuf(&buffer, f, BUFSIZ)) == 0) {
			(void)close(f);
			continue;
		}
		readerr = 0;
		for (i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (readerr == 0 && read(f, bp->buf, amt) != amt)
				readerr = errno;
			(void)write(rem, bp->buf, amt);
		}
		(void)close(f);
		if (readerr == 0)
			(void)write(rem, "", 1);
		else
			error("rcp: %s: %s\n", name, strerror(readerr));
		(void)response();
	}
}

rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[MAXPATHLEN];

	if (!(dirp = opendir(name))) {
		error("rcp: %s: %s\n", name, strerror(errno));
		return;
	}
	last = rindex(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void)snprintf(path, sizeof(path),
		    "T%ld 0 %ld 0\n", statp->st_mtime, statp->st_atime);
		(void)write(rem, path, (int)strlen(path));
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	(void)snprintf(path, sizeof(path),
	    "D%04o %d %s\n", statp->st_mode&07777, 0, last);
	(void)write(rem, path, (int)strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while (dp = readdir(dirp)) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= MAXPATHLEN - 1) {
			error("%s/%s: name too long.\n", name, dp->d_name);
			continue;
		}
		(void)snprintf(path, sizeof(path), "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	closedir(dirp);
	(void)write(rem, "E\n", 2);
	(void)response();
}

response()
{
	register char *cp;
	char ch, resp, rbuf[BUFSIZ];

	if (read(rem, &resp, sizeof(resp)) != sizeof(resp))
		lostconn();

	cp = rbuf;
	switch(resp) {
	case 0:				/* ok */
		return(0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by err msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				lostconn();
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			(void)write(2, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return(-1);
		exit(1);
	}
	/*NOTREACHED*/
}

void
lostconn()
{
	if (!iamremote)
		(void)fprintf(stderr, "rcp: lost connection\n");
	exit(1);
}

sink(argc, argv)
	int argc;
	char **argv;
{
	register char *cp;
	static BUF buffer;
	struct stat stb;
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	BUF *bp, *allocbuf();
	off_t i, j;
	char ch, *targ, *why;
	int amt, count, exists, first, mask, mode;
	int ofd, setimes, size, targisdir;
	char *np, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void)umask(mask);
	if (argc != 1) {
		error("rcp: ambiguous target\n");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	(void)write(rem, "", 1);
	if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void)write(2, buf + 1, (int)strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			errs++;
			continue;
		}
		if (buf[0] == 'E') {
			(void)write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void)write(rem, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				error("%s\n", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");
		size = 0;
		while (isdigit(*cp))
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc(need)))
					error("out of memory\n");
			}
			(void)snprintf(namebuf, need, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		}
		else
			np = targ;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			if (exists) {
				if ((stb.st_mode&S_IFMT) != S_IFDIR) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void)chmod(np, mode);
			} else if (mkdir(np, mode) < 0)
				goto bad;
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(np, tv) < 0)
				    error("rcp: can't set times on %s: %s\n",
					np, strerror(errno));
			}
			continue;
		}
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			error("rcp: %s: %s\n", np, strerror(errno));
			continue;
		}
		if (exists && pflag)
			(void)fchmod(ofd, mode);
		(void)write(rem, "", 1);
		if ((bp = allocbuf(&buffer, ofd, BUFSIZ)) == 0) {
			(void)close(ofd);
			continue;
		}
		cp = bp->buf;
		count = 0;
		wrerr = NO;
		for (i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					error("rcp: %s\n",
					    j ? strerror(errno) :
					    "dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				if (wrerr == NO &&
				    write(ofd, bp->buf, count) != count)
					wrerr = YES;
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == NO &&
		    write(ofd, bp->buf, count) != count)
			wrerr = YES;
		if (ftruncate(ofd, size)) {
			error("rcp: can't truncate %s: %s\n", np,
			    strerror(errno));
			wrerr = DISPLAYED;
		}
		(void)close(ofd);
		(void)response();
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				error("rcp: can't set times on %s: %s\n",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch(wrerr) {
		case YES:
			error("rcp: %s: %s\n", np, strerror(errno));
			break;
		case NO:
			(void)write(rem, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	error("rcp: protocol screwup: %s\n", why);
	exit(1);
}

BUF *
allocbuf(bp, fd, blksize)
	BUF *bp;
	int fd, blksize;
{
	struct stat stb;
	size_t size;

	if (fstat(fd, &stb) < 0) {
		error("rcp: fstat: %s\n", strerror(errno));
		return(0);
	}
	size = roundup(stb.st_blksize, blksize);
	if (size == 0)
		size = blksize;
	if (bp->cnt < size) {
		if (bp->buf != 0)
			free(bp->buf);
		bp->buf = malloc(size);
		if (!bp->buf) {
			error("rcp: malloc: out of memory\n");
			return(0);
		}
	}
	bp->cnt = size;
	return(bp);
}

/* VARARGS1 */
error(fmt, a1, a2, a3)
	char *fmt;
	int a1, a2, a3;
{
	static FILE *fp;

	++errs;
	if (!fp && !(fp = fdopen(rem, "w")))
		return;
	(void)fprintf(fp, "%c", 0x01);
	(void)fprintf(fp, fmt, a1, a2, a3);
	(void)fflush(fp);
	if (!iamremote)
		(void)fprintf(stderr, fmt, a1, a2, a3);
}

nospace()
{
	(void)fprintf(stderr, "rcp: out of memory.\n");
	exit(1);
}


usage()
{
#ifdef KERBEROS
#ifdef CRYPT
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: rcp [-k realm] [-px] f1 f2",
	    "or: rcp [-k realm] [-rpx] f1 ... fn directory");
#else
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: rcp [-k realm] [-p] f1 f2",
	    "or: rcp [-k realm] [-rp] f1 ... fn directory");
#endif
#else
	(void)fprintf(stderr,
	    "usage: rcp [-p] f1 f2; or: rcp [-rp] f1 ... fn directory\n");
#endif
	exit(1);
}

#ifdef KERBEROS
old_warning(str)
	char *str;
{
	(void)fprintf(stderr, "rcp: warning: %s, using standard rcp\n", str);
}

int
kerberos(host, bp, locuser, user)

	char **host, *bp, *locuser, *user;
{
	struct servent *sp;

again:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(*host);

#ifdef CRYPT
		if (doencrypt)
			rem = krcmd_mutual(
				host, port,
				user, bp, 0,
		    		dest_realm,
				&cred, schedule);
		else
#endif
			rem = krcmd(
				host, port,
				user, bp, 0, dest_realm);

		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("shell", "tcp");
			if (sp == NULL) {
				(void)fprintf(stderr,
			    	    "rcp: unknown service shell/tcp\n");
				exit(1);
			}
			if (errno == ECONNREFUSED)
				old_warning(
				    "remote host doesn't support Kerberos");

			if (errno == ENOENT)
				old_warning(
				    "Can't provide Kerberos auth data");
			port = sp->s_port;
			goto again;
		}
	} else {
#ifdef CRYPT
		if (doencrypt) {
			fprintf(stderr,
			    "The -x option requires Kerberos authentication\n");
			exit(1);
		}
#endif
		rem = rcmd(host, sp->s_port, locuser, user, bp, 0);
	}
	return(rem);
}
#endif /* KERBEROS */
