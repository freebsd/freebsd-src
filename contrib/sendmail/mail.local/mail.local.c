/*-
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mail.local.c	8.78 (Berkeley) 5/19/98";
#endif /* not lint */

/*
 * This is not intended to work on System V derived systems
 * such as Solaris or HP-UX, since they use a totally different
 * approach to mailboxes (essentially, they have a setgid program
 * rather than setuid, and they rely on the ability to "give away"
 * files to do their work).  IT IS NOT A BUG that this doesn't
 * work on such architectures.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#ifdef EX_OK
# undef EX_OK		/* unistd.h may have another use for this */
#endif
#include <sysexits.h>
#include <ctype.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if (defined(sun) && defined(__svr4__)) || defined(__SVR4)
# define USE_LOCKF	1
# define USE_SETEUID	1
# define _PATH_MAILDIR	"/var/mail"
#endif

#if (defined(sun) && !defined(__svr4__)) && !defined(__SVR4)
# ifdef __dead
#  undef __dead
#  define __dead
# endif
#endif

#if defined(_AIX)
# define USE_LOCKF	1
# define USE_SETEUID	1
# define USE_VSYSLOG	0
#endif

#if defined(__hpux)
# define USE_LOCKF	1
# define USE_SETRESUID	1
# define USE_VSYSLOG	0
# ifdef __dead
#  undef __dead
#  define __dead
# endif
#endif

#if defined(_CRAY)
# if !defined(MAXPATHLEN)
#  define MAXPATHLEN PATHSIZE
# endif
# define USE_VSYSLOG   0
# define _PATH_MAILDIR	"/usr/spool/mail"
#endif

#if defined(ultrix)
# define USE_VSYSLOG	0
#endif

#if defined(__osf__)
# define USE_VSYSLOG	0
#endif

#if defined(NeXT)
# include <libc.h>
# define _PATH_MAILDIR	"/usr/spool/mail"
# define __dead		/* empty */
# define S_IRUSR	S_IREAD
# define S_IWUSR	S_IWRITE
#endif

#if defined(IRIX64) || defined(IRIX5) || defined(IRIX6)
# include <paths.h>
# define HASSTRERROR	1	/* has strerror(3) */
#endif

/*
 * If you don't have flock, you could try using lockf instead.
 */

#ifdef USE_LOCKF
# define flock(a, b)	lockf(a, b, 0)
# define LOCK_EX	F_LOCK
#endif

#ifndef USE_VSYSLOG
# define USE_VSYSLOG	1
#endif

#ifndef LOCK_EX
# include <sys/file.h>
#endif

#if defined(BSD4_4) || defined(__GLIBC__)
# include "pathnames.h"
#endif

#ifndef __P
# ifdef __STDC__
#  define __P(protos)	protos
# else
#  define __P(protos)	()
#  define const
# endif
#endif
#ifndef __dead
# if defined(__GNUC__) && (__GNUC__ < 2 || __GNUC_MINOR__ < 5) && !defined(__STRICT_ANSI__)
#  define __dead	__volatile
# else
#  define __dead
# endif
#endif

#ifdef BSD4_4
# define HAS_ST_GEN	1
#else
# ifndef _BSD_VA_LIST_
#  define _BSD_VA_LIST_	va_list
# endif
#endif

#if defined(BSD4_4) || defined(linux)
# define HASSNPRINTF	1
#else
# ifndef ultrix
extern FILE	*fdopen __P((int, const char *));
# endif
#endif

#if SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206)
# define HASSNPRINTF	1		/* has snprintf starting in 2.6 */
#endif

#if !HASSNPRINTF
extern int	snprintf __P((char *, size_t, const char *, ...));
# ifndef _CRAY
extern int	vsnprintf __P((char *, size_t, const char *, ...));
# endif
#endif

#if defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__)
# ifndef HASSTRERROR
#  define HASSTRERROR	1
# endif
#endif

#if !HASSTRERROR
extern char	*strerror __P((int));
#endif

/*
 * If you don't have setreuid, and you have saved uids, and you have
 * a seteuid() call that doesn't try to emulate using setuid(), then
 * you can try defining USE_SETEUID.
 */
#ifdef USE_SETEUID
# define setreuid(r, e)		seteuid(e)
#endif

/*
 * And of course on hpux you have setresuid()
 */
#ifdef USE_SETRESUID
# define setreuid(r, e)		setresuid(-1, e, -1)
#endif

#ifndef _PATH_LOCTMP
# define _PATH_LOCTMP	"/tmp/local.XXXXXX"
#endif
#ifndef _PATH_MAILDIR
# define _PATH_MAILDIR	"/var/spool/mail"
#endif

#ifndef S_ISREG
# define S_ISREG(mode)	(((mode) & _S_IFMT) == S_IFREG)
#endif

int	eval = EX_OK;			/* sysexits.h error value. */
int	lmtpmode = 0;
u_char	tTdvect[100];

void		deliver __P((int, char *, int, int));
void		e_to_sys __P((int));
void		err __P((const char *, ...)) __dead2;
void		notifybiff __P((char *));
int		store __P((char *, int));
void		usage __P((void));
void		vwarn __P((const char *, _BSD_VA_LIST_));
void		warn __P((const char *, ...));
void		lockmbox __P((char *));
void		unlockmbox __P((void));
void		mailerr __P((const char *, const char *, ...));
void		dolmtp __P((int, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct passwd *pw;
	int ch, fd, nobiff, dofsync;
	uid_t uid;
	char *from;
	extern char *optarg;
	extern int optind;

	/* make sure we have some open file descriptors */
	for (fd = 10; fd < 30; fd++)
		(void) close(fd);

	/* use a reasonable umask */
	(void) umask(0077);

#ifdef LOG_MAIL
	openlog("mail.local", 0, LOG_MAIL);
#else
	openlog("mail.local", 0);
#endif

	from = NULL;
	nobiff = 0;
	dofsync = 0;
	while ((ch = getopt(argc, argv, "bdf:r:ls")) != -1)
		switch(ch) {
		case 'b':
			nobiff++;
			break;
		case 'd':		/* Backward compatible. */
			break;
		case 'f':
		case 'r':		/* Backward compatible. */
			if (from != NULL) {
				warn("multiple -f options");
				usage();
			}
			from = optarg;
			break;
		case 'l':
			lmtpmode++;
			break;
		case 's':
			dofsync++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (lmtpmode)
		dolmtp(nobiff, dofsync);

	if (!*argv)
		usage();

	/*
	 * If from not specified, use the name from getlogin() if the
	 * uid matches, otherwise, use the name from the password file
	 * corresponding to the uid.
	 */
	uid = getuid();
	if (!from && (!(from = getlogin()) ||
	    !(pw = getpwnam(from)) || pw->pw_uid != uid))
		from = (pw = getpwuid(uid)) ? pw->pw_name : "???";

	/*
	 * There is no way to distinguish the error status of one delivery
	 * from the rest of the deliveries.  So, if we failed hard on one
	 * or more deliveries, but had no failures on any of the others, we
	 * return a hard failure.  If we failed temporarily on one or more
	 * deliveries, we return a temporary failure regardless of the other
	 * failures.  This results in the delivery being reattempted later
	 * at the expense of repeated failures and multiple deliveries.
	 */
	for (fd = store(from, 0); *argv; ++argv)
		deliver(fd, *argv, nobiff, dofsync);
	exit(eval);
}

char *
parseaddr(s)
	char *s;
{
	char *p;
	int len;

	if (*s++ != '<')
		return NULL;

	p = s;

	/* at-domain-list */
	while (*p == '@') {
		p++;
		if (*p == '[') {
			p++;
			while (isascii(*p) &&
			       (isalnum(*p) || *p == '.' ||
				*p == '-' || *p == ':'))
				p++;
			if (*p++ != ']')
				return NULL;
		} else {
			while ((isascii(*p) && isalnum(*p)) ||
			       *p == '.' || *p == '-')
				p++;
		}
		if (*p == ',' && p[1] == '@')
			p++;
		else if (*p == ':' && p[1] != '@')
			p++;
		else
			return NULL;
	}

	/* local-part */
	if (*p == '\"') {
		p++;
		while (*p && *p != '\"') {
			if (*p == '\\') {
				if (!*++p)
					return NULL;
			}
			p++;
		}
		if (!*p++)
			return NULL;
	} else {
		while (*p && *p != '@' && *p != '>') {
			if (*p == '\\') {
				if (!*++p)
					return NULL;
			} else {
			if (*p <= ' ' || (*p & 128) ||
			    strchr("<>()[]\\,;:\"", *p))
				return NULL;
			}
			p++;
		}
	}

	/* @domain */
	if (*p == '@') {
		p++;
		if (*p == '[') {
			p++;
			while (isascii(*p) &&
			       (isalnum(*p) || *p == '.' ||
				*p == '-' || *p == ':'))
				p++;
			if (*p++ != ']')
				return NULL;
		} else {
			while ((isascii(*p) && isalnum(*p)) ||
			       *p == '.' || *p == '-')
				p++;
		}
	}

	if (*p++ != '>')
		return NULL;
	if (*p && *p != ' ')
		return NULL;
	len = p - s - 1;

	p = malloc(len + 1);
	if (p == NULL) {
		printf("421 4.3.0 memory exhausted\r\n");
		exit(EX_TEMPFAIL);
	}

	strncpy(p, s, len);
	p[len] = '\0';
	return p;
}

char *
process_recipient(addr)
	char *addr;
{
	if (getpwnam(addr) == NULL) {
		return "550 5.1.1 user unknown";
	}

	return NULL;
}


#define RCPT_GROW	30

void
dolmtp(nobiff, dofsync)
	int nobiff, dofsync;
{
	char *return_path = NULL;
	char **rcpt_addr = NULL;
	int rcpt_num = 0;
	int rcpt_alloc = 0;
	char myhostname[1024];
	char buf[4096];
	char *err;
	int msgfd;
	char *p;
	int i;

	gethostname(myhostname, sizeof myhostname - 1);

	printf("220 %s LMTP ready\r\n", myhostname);
	for (;;) {
		fflush(stdout);
		if (fgets(buf, sizeof(buf)-1, stdin) == NULL) {
			exit(EX_OK);
		}
		p = buf + strlen(buf) - 1;
		if (p >= buf && *p == '\n')
			*p-- = '\0';
		if (p >= buf && *p == '\r')
			*p-- = '\0';

		switch (buf[0]) {

		case 'd':
		case 'D':
			if (strcasecmp(buf, "data") == 0) {
				if (rcpt_num == 0) {
					printf("503 5.5.1 No recipients\r\n");
					continue;
				}
				msgfd = store(return_path, rcpt_num);
				if (msgfd == -1)
					continue;

				for (i = 0; i < rcpt_num; i++) {
					p = strchr(rcpt_addr[i], '+');
					if (p != NULL)
						*p++ = '\0';
					deliver(msgfd, rcpt_addr[i], nobiff,
					    dofsync);
				}
				close(msgfd);
				goto rset;
			}
			goto syntaxerr;

		case 'l':
		case 'L':
			if (strncasecmp(buf, "lhlo ", 5) == 0) {
				printf("250-%s\r\n250-8BITMIME\r\n250-ENHANCEDSTATUSCODES\r\n250 PIPELINING\r\n",
					   myhostname);
				continue;
			}
			goto syntaxerr;

		case 'm':
		case 'M':
			if (strncasecmp(buf, "mail ", 5) == 0) {
				if (return_path != NULL) {
					printf("503 5.5.1 Nested MAIL command\r\n");
					continue;
				}
				if (strncasecmp(buf+5, "from:", 5) != 0 ||
				    ((return_path = parseaddr(buf+10)) == NULL)) {
					printf("501 5.5.4 Syntax error in parameters\r\n");
					continue;
				}
				printf("250 2.5.0 ok\r\n");
				continue;
			}
			goto syntaxerr;

		case 'n':
		case 'N':
			if (strcasecmp(buf, "noop") == 0) {
				printf("250 2.0.0 ok\r\n");
				continue;
			}
			goto syntaxerr;

		case 'q':
		case 'Q':
			if (strcasecmp(buf, "quit") == 0) {
				printf("221 2.0.0 bye\r\n");
				exit(EX_OK);
			}
			goto syntaxerr;

		case 'r':
		case 'R':
			if (strncasecmp(buf, "rcpt ", 5) == 0) {
				if (return_path == NULL) {
					printf("503 5.5.1 Need MAIL command\r\n");
					continue;
				}
				if (rcpt_num >= rcpt_alloc) {
					rcpt_alloc += RCPT_GROW;
					rcpt_addr = (char **)
						realloc((char *)rcpt_addr,
							rcpt_alloc * sizeof(char **));
					if (rcpt_addr == NULL) {
						printf("421 4.3.0 memory exhausted\r\n");
						exit(EX_TEMPFAIL);
					}
				}
				if (strncasecmp(buf+5, "to:", 3) != 0 ||
				    ((rcpt_addr[rcpt_num] = parseaddr(buf+8)) == NULL)) {
					printf("501 5.5.4 Syntax error in parameters\r\n");
					continue;
				}
				if ((err = process_recipient(rcpt_addr[rcpt_num])) != NULL) {
					printf("%s\r\n", err);
					continue;
				}
				rcpt_num++;
				printf("250 2.1.5 ok\r\n");
				continue;
			}
			else if (strcasecmp(buf, "rset") == 0) {
				printf("250 2.0.0 ok\r\n");

  rset:
				while (rcpt_num) {
					free(rcpt_addr[--rcpt_num]);
				}
				if (return_path != NULL)
					free(return_path);
				return_path = NULL;
				continue;
			}
			goto syntaxerr;

		case 'v':
		case 'V':
			if (strncasecmp(buf, "vrfy ", 5) == 0) {
				printf("252 2.3.3 try RCPT to attempt delivery\r\n");
				continue;
			}
			goto syntaxerr;

		default:
  syntaxerr:
			printf("500 5.5.2 Syntax error\r\n");
			continue;
		}
	}
}

int
store(from, lmtprcpts)
	char *from;
	int lmtprcpts;
{
	FILE *fp;
	time_t tval;
	int fd, eline;
	char line[2048];
	char tmpbuf[sizeof _PATH_LOCTMP + 1];

	strcpy(tmpbuf, _PATH_LOCTMP);
	if ((fd = mkstemp(tmpbuf)) == -1 || (fp = fdopen(fd, "w+")) == NULL) {
		if (lmtprcpts) {
			printf("451 4.3.0 unable to open temporary file\r\n");
			return -1;
		} else {
			e_to_sys(errno);
			err("unable to open temporary file");
		}
	}
	(void)unlink(tmpbuf);

	if (lmtpmode) {
		printf("354 go ahead\r\n");
		fflush(stdout);
	}

	(void)time(&tval);
	(void)fprintf(fp, "From %s %s", from, ctime(&tval));

	line[0] = '\0';
	for (eline = 1; fgets(line, sizeof(line), stdin);) {
		if (line[strlen(line)-2] == '\r') {
			strcpy(line+strlen(line)-2, "\n");
		}
		if (lmtprcpts && line[0] == '.') {
			if (line[1] == '\n')
				goto lmtpdot;
			strcpy(line, line+1);
		}
		if (line[0] == '\n')
			eline = 1;
		else {
			if (eline && line[0] == 'F' &&
			    !memcmp(line, "From ", 5))
				(void)putc('>', fp);
			eline = 0;
		}
		(void)fprintf(fp, "%s", line);
		if (ferror(fp)) {
			if (lmtprcpts) {
				while (lmtprcpts--) {
					printf("451 4.3.0 temporary file write error\r\n");
				}
				fclose(fp);
				return -1;
			} else {
				e_to_sys(errno);
				err("temporary file write error");
			}
		}
	}

	if (lmtprcpts) {
		/* Got a premature EOF -- toss message and exit */
		exit(EX_OK);
	}

	/* If message not newline terminated, need an extra. */
	if (strchr(line, '\n') == NULL)
		(void)putc('\n', fp);

  lmtpdot:

	/* Output a newline; note, empty messages are allowed. */
	(void)putc('\n', fp);

	if (fflush(fp) == EOF || ferror(fp)) {
		if (lmtprcpts) {
			while (lmtprcpts--) {
				printf("451 4.3.0 temporary file write error\r\n");
			}
			fclose(fp);
			return -1;
		} else {
			e_to_sys(errno);
			err("temporary file write error");
		}
	}
	return (fd);
}

void
deliver(fd, name, nobiff, dofsync)
	int fd;
	char *name;
	int nobiff, dofsync;
{
	struct stat fsb, sb;
	struct passwd *pw;
	int mbfd, nr, nw, off;
	char *p;
	char biffmsg[100], buf[8*1024], path[MAXPATHLEN];
	off_t curoff;
	extern char *quad_to_string();

	/*
	 * Disallow delivery to unknown names -- special mailboxes can be
	 * handled in the sendmail aliases file.
	 */
	if ((pw = getpwnam(name)) == NULL) {
		if (eval != EX_TEMPFAIL)
			eval = EX_UNAVAILABLE;
		if (lmtpmode) {
			if (eval == EX_TEMPFAIL) {
				printf("451 4.3.0 cannot lookup name: %s\r\n", name);
			} else {
				printf("550 5.1.1 unknown name: %s\r\n", name);
			}
		}
		else {
			warn("unknown name: %s", name);
		}
		return;
	}
	endpwent();

	/*
	 * Keep name reasonably short to avoid buffer overruns.
	 *	This isn't necessary on BSD because of the proper
	 *	definition of snprintf(), but it can cause problems
	 *	on other systems.
	 * Also, clear out any bogus characters.
	 */

	if (strlen(name) > 40)
		name[40] = '\0';
	for (p = name; *p != '\0'; p++)
	{
		if (!isascii(*p))
			*p &= 0x7f;
		else if (!isprint(*p))
			*p = '.';
	}

	(void)snprintf(path, sizeof(path), "%s/%s", _PATH_MAILDIR, name);

	/*
	 * If the mailbox is linked or a symlink, fail.  There's an obvious
	 * race here, that the file was replaced with a symbolic link after
	 * the lstat returned, but before the open.  We attempt to detect
	 * this by comparing the original stat information and information
	 * returned by an fstat of the file descriptor returned by the open.
	 *
	 * NB: this is a symptom of a larger problem, that the mail spooling
	 * directory is writeable by the wrong users.  If that directory is
	 * writeable, system security is compromised for other reasons, and
	 * it cannot be fixed here.
	 *
	 * If we created the mailbox, set the owner/group.  If that fails,
	 * just return.  Another process may have already opened it, so we
	 * can't unlink it.  Historically, binmail set the owner/group at
	 * each mail delivery.  We no longer do this, assuming that if the
	 * ownership or permissions were changed there was a reason.
	 *
	 * XXX
	 * open(2) should support flock'ing the file.
	 */
tryagain:
	lockmbox(path);
	if (lstat(path, &sb) < 0) {
		mbfd = open(path,
			O_APPEND|O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR);
		if (lstat(path, &sb) < 0)
		{
			eval = EX_CANTCREAT;
			warn("%s: lstat: file changed after open", path);
			goto err1;
		}
		else
			sb.st_uid = pw->pw_uid;
		if (mbfd == -1) {
			if (errno == EEXIST)
				goto tryagain;
		} else if (fchown(mbfd, pw->pw_uid, pw->pw_gid)) {
			mailerr("451 4.3.0", "chown %u.%u: %s",
				pw->pw_uid, pw->pw_gid, name);
			goto err1;
		}
	} else if (sb.st_nlink != 1 || !S_ISREG(sb.st_mode)) {
		mailerr("550 5.2.0", "%s: irregular file", path);
		goto err0;
	} else if (sb.st_uid != pw->pw_uid) {
		eval = EX_CANTCREAT;
		mailerr("550 5.2.0", "%s: wrong ownership (%d)",
				path, sb.st_uid);
		goto err0;
	} else {
		mbfd = open(path, O_APPEND|O_WRONLY, 0);
	}

	if (mbfd == -1) {
		mailerr("450 4.2.0", "%s: %s", path, strerror(errno));
		goto err0;
	} else if (fstat(mbfd, &fsb) < 0 ||
	    fsb.st_nlink != 1 ||
	    sb.st_nlink != 1 ||
	    !S_ISREG(fsb.st_mode) ||
	    sb.st_dev != fsb.st_dev ||
	    sb.st_ino != fsb.st_ino ||
#if HAS_ST_GEN && 0		/* AFS returns random values for st_gen */
	    sb.st_gen != fsb.st_gen ||
#endif
	    sb.st_uid != fsb.st_uid) {
		eval = EX_TEMPFAIL;
		warn("%s: fstat: file changed after open", path);
		goto err1;
	}

	/* Wait until we can get a lock on the file. */
	if (flock(mbfd, LOCK_EX)) {
		mailerr("450 4.2.0", "%s: %s", path, strerror(errno));
		goto err1;
	}

	if (!nobiff) {
		/* Get the starting offset of the new message for biff. */
		curoff = lseek(mbfd, (off_t)0, SEEK_END);
		if (sizeof curoff > sizeof(long))
			(void)snprintf(biffmsg, sizeof(biffmsg), "%s@%s\n",
				       name, quad_to_string(curoff));
		else
			(void)snprintf(biffmsg, sizeof(biffmsg), "%s@%ld\n",
				       name, curoff);
	}

	/* Copy the message into the file. */
	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		mailerr("450 4.2.0", "temporary file: %s",
			strerror(errno));
		goto err1;
	}
	if (setreuid(0, pw->pw_uid) < 0) {
		mailerr("450 4.2.0", "setreuid(0, %d): %s (r=%d, e=%d)",
		     pw->pw_uid, strerror(errno), getuid(), geteuid());
		goto err1;
	}
#ifdef DEBUG
	printf("new euid = %d\n", geteuid());
#endif
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; off += nw)
			if ((nw = write(mbfd, buf + off, nr - off)) < 0) {
				mailerr("450 4.2.0", "%s: %s",
					path, strerror(errno));
				goto err3;
			}
	if (nr < 0) {
		mailerr("450 4.2.0", "temporary file: %s",
			strerror(errno));
		goto err3;
	}

	/* Flush to disk, don't wait for update. */
	if (dofsync && fsync(mbfd)) {
		mailerr("450 4.2.0", "%s: %s", path, strerror(errno));
err3:
		if (setreuid(0, 0) < 0) {
			e_to_sys(errno);
			mailerr("450 4.2.0", "setreuid(0, 0): %s",
				strerror(errno));
		}
#ifdef DEBUG
		printf("reset euid = %d\n", geteuid());
#endif
		(void)ftruncate(mbfd, curoff);
err1:		(void)close(mbfd);
err0:		unlockmbox();
		return;
	}

	/* Close and check -- NFS doesn't write until the close. */
	if (close(mbfd)) {
		mailerr("450 4.2.0", "%s: %s", path, strerror(errno));
		truncate(path, curoff);
	} else if (!nobiff)
		notifybiff(biffmsg);

	if (setreuid(0, 0) < 0) {
		mailerr("450 4.2.0", "setreuid(0, 0): %s",
			strerror(errno));
		goto err0;
	}
#ifdef DEBUG
	printf("reset euid = %d\n", geteuid());
#endif
	unlockmbox();
	if (lmtpmode) {
		printf("250 2.1.5 %s OK\r\n", name);
	}
}

/*
 * user.lock files are necessary for compatibility with other
 * systems, e.g., when the mail spool file is NFS exported.
 * Alas, mailbox locking is more than just a local matter.
 * EPA 11/94.
 */

char	lockname[MAXPATHLEN];
int	locked = 0;

void
lockmbox(path)
	char *path;
{
	int statfailed = 0;

	if (locked)
		return;
	if (strlen(path) + 6 > sizeof lockname)
		return;
	snprintf(lockname, sizeof lockname, "%s.lock", path);
	for (;; sleep(5)) {
		int fd;
		struct stat st;
		time_t now;

		fd = open(lockname, O_WRONLY|O_EXCL|O_CREAT, 0);
		if (fd >= 0) {
			/* defeat lock checking programs which test pid */
			write(fd, "0", 2);
			locked = 1;
			close(fd);
			return;
		}
		if (stat(lockname, &st) < 0) {
			if (statfailed++ > 5)
				return;
			continue;
		}
		statfailed = 0;
		time(&now);
		if (now < st.st_ctime + 300)
			continue;
		unlink(lockname);
	}
}

void
unlockmbox()
{
	if (!locked)
		return;
	unlink(lockname);
	locked = 0;
}

void
notifybiff(msg)
	char *msg;
{
	static struct sockaddr_in addr;
	static int f = -1;
	struct hostent *hp;
	struct servent *sp;
	int len;

	if (addr.sin_family == 0) {
		/* Be silent if biff service not available. */
		if ((sp = getservbyname("biff", "udp")) == NULL)
			return;
		if ((hp = gethostbyname("localhost")) == NULL) {
			warn("localhost: %s", strerror(errno));
			return;
		}
		addr.sin_family = hp->h_addrtype;
		memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
		addr.sin_port = sp->s_port;
	}
	if (f < 0 && (f = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		warn("socket: %s", strerror(errno));
		return;
	}
	len = strlen(msg) + 1;
	if (sendto(f, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr))
	    != len)
		warn("sendto biff: %s", strerror(errno));
}

void
usage()
{
	eval = EX_USAGE;
	err("usage: mail.local [-b] [-l] [-f from] [-s] user ...");
}

void
#ifdef __STDC__
mailerr(const char *hdr, const char *fmt, ...)
#else
mailerr(hdr, fmt, va_alist)
	const char *hdr;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (lmtpmode)
	{
		printf("%s ", hdr);
		vprintf(fmt, ap);
		printf("\r\n");
	}
	else
	{
		e_to_sys(errno);
		vwarn(fmt, ap);
	}
}

#ifdef __STDC__
void
err(const char *fmt, ...)
#else
void
err(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarn(fmt, ap);
	va_end(ap);

	exit(eval);
}

void
#ifdef __STDC__
warn(const char *fmt, ...)
#else
warn(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarn(fmt, ap);
	va_end(ap);
}

void
vwarn(fmt, ap)
	const char *fmt;
	_BSD_VA_LIST_ ap;
{
	/*
	 * Log the message to stderr.
	 *
	 * Don't use LOG_PERROR as an openlog() flag to do this,
	 * it's not portable enough.
	 */
	if (eval != EX_USAGE)
		(void)fprintf(stderr, "mail.local: ");
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");

#if USE_VSYSLOG
	/* Log the message to syslog. */
	vsyslog(LOG_ERR, fmt, ap);
#else
	{
		char fmtbuf[10240];

		(void) vsnprintf(fmtbuf, sizeof fmtbuf, fmt, ap);
		syslog(LOG_ERR, "%s", fmtbuf);
	}
#endif
}

/*
 * e_to_sys --
 *	Guess which errno's are temporary.  Gag me.
 */
void
e_to_sys(num)
	int num;
{
	/* Temporary failures override hard errors. */
	if (eval == EX_TEMPFAIL)
		return;

	switch(num) {		/* Hopefully temporary errors. */
#ifdef EAGAIN
	case EAGAIN:		/* Resource temporarily unavailable */
#endif
#ifdef EDQUOT
	case EDQUOT:		/* Disc quota exceeded */
#endif
#ifdef EBUSY
	case EBUSY:		/* Device busy */
#endif
#ifdef EPROCLIM
	case EPROCLIM:		/* Too many processes */
#endif
#ifdef EUSERS
	case EUSERS:		/* Too many users */
#endif
#ifdef ECONNABORTED
	case ECONNABORTED:	/* Software caused connection abort */
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:	/* Connection refused */
#endif
#ifdef ECONNRESET
	case ECONNRESET:	/* Connection reset by peer */
#endif
#ifdef EDEADLK
	case EDEADLK:		/* Resource deadlock avoided */
#endif
#ifdef EFBIG
	case EFBIG:		/* File too large */
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN:		/* Host is down */
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:	/* No route to host */
#endif
#ifdef EMFILE
	case EMFILE:		/* Too many open files */
#endif
#ifdef ENETDOWN
	case ENETDOWN:		/* Network is down */
#endif
#ifdef ENETRESET
	case ENETRESET:		/* Network dropped connection on reset */
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:	/* Network is unreachable */
#endif
#ifdef ENFILE
	case ENFILE:		/* Too many open files in system */
#endif
#ifdef ENOBUFS
	case ENOBUFS:		/* No buffer space available */
#endif
#ifdef ENOMEM
	case ENOMEM:		/* Cannot allocate memory */
#endif
#ifdef ENOSPC
	case ENOSPC:		/* No space left on device */
#endif
#ifdef EROFS
	case EROFS:		/* Read-only file system */
#endif
#ifdef ESTALE
	case ESTALE:		/* Stale NFS file handle */
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:		/* Connection timed out */
#endif
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN && EWOULDBLOCK != EDEADLK
	case EWOULDBLOCK:	/* Operation would block. */
#endif
		eval = EX_TEMPFAIL;
		break;
	default:
		eval = EX_UNAVAILABLE;
		break;
	}
}

#if !HASSTRERROR

char *
strerror(eno)
	int eno;
{
	extern int sys_nerr;
	extern char *sys_errlist[];
	static char ebuf[60];

	if (eno >= 0 && eno < sys_nerr)
		return sys_errlist[eno];
	(void) sprintf(ebuf, "Error %d", eno);
	return ebuf;
}

#endif /* !HASSTRERROR */

#if defined(ultrix) || defined(_CRAY)

/*
 * Copyright (c) 1987, 1993
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
static char sccsid[] = "@(#)mktemp.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

static int _gettemp();

mkstemp(path)
	char *path;
{
	int fd;

	return (_gettemp(path, &fd) ? fd : -1);
}

/*
char *
mktemp(path)
	char *path;
{
	return(_gettemp(path, (int *)NULL) ? path : (char *)NULL);
}
*/

static
_gettemp(path, doopen)
	char *path;
	register int *doopen;
{
	extern int errno;
	register char *start, *trv;
	struct stat sbuf;
	u_int pid;

	pid = getpid();
	for (trv = path; *trv; ++trv);		/* extra X's get set to 0's */
	while (*--trv == 'X') {
		*trv = (pid % 10) + '0';
		pid /= 10;
	}

	/*
	 * check the target directory; if you have six X's and it
	 * doesn't exist this runs for a *very* long time.
	 */
	for (start = trv + 1;; --trv) {
		if (trv <= path)
			break;
		if (*trv == '/') {
			*trv = '\0';
			if (stat(path, &sbuf) < 0)
				return(0);
			if (!S_ISDIR(sbuf.st_mode)) {
				errno = ENOTDIR;
				return(0);
			}
			*trv = '/';
			break;
		}
	}

	for (;;) {
		if (doopen) {
			if ((*doopen =
			    open(path, O_CREAT|O_EXCL|O_RDWR, 0600)) >= 0)
				return(1);
			if (errno != EEXIST)
				return(0);
		}
		else if (stat(path, &sbuf) < 0)
			return(errno == ENOENT ? 1 : 0);

		/* tricky little algorithm for backward compatibility */
		for (trv = start;;) {
			if (!*trv)
				return(0);
			if (*trv == 'z')
				*trv++ = 'a';
			else {
				if (isascii(*trv) && isdigit(*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/*NOTREACHED*/
}

#endif /* ultrix */
