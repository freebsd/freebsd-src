/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
"@(#) Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: mail.local.c,v 8.143.4.58 2001/06/01 05:33:31 gshapiro Exp $";
#endif /* ! lint */

/* $FreeBSD$ */

/*
**  This is not intended to work on System V derived systems
**  such as Solaris or HP-UX, since they use a totally different
**  approach to mailboxes (essentially, they have a setgid program
**  rather than setuid, and they rely on the ability to "give away"
**  files to do their work).  IT IS NOT A BUG that this doesn't
**  work on such architectures.
*/


/* additional mode for open() */
# define EXTRA_MODE 0

# include <sys/types.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <sys/socket.h>
# include <sys/file.h>

# include <netinet/in.h>
# include <arpa/nameser.h>

# include <fcntl.h>
# include <netdb.h>
#  include <pwd.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <syslog.h>
# include <time.h>
# include <unistd.h>
# ifdef EX_OK
#  undef EX_OK		/* unistd.h may have another use for this */
# endif /* EX_OK */
# include <sysexits.h>
# include <ctype.h>

# ifndef __P
#  include "sendmail/cdefs.h"
# endif /* ! __P */
# include "sendmail/useful.h"

extern size_t	strlcpy __P((char *, const char *, size_t));
extern size_t	strlcat __P((char *, const char *, size_t));

# if defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__) || defined(IRIX64) || defined(IRIX5) || defined(IRIX6)
#  ifndef HASSTRERROR
#   define HASSTRERROR	1
#  endif /* ! HASSTRERROR */
# endif /* defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__) || defined(IRIX64) || defined(IRIX5) || defined(IRIX6) */

# include "sendmail/errstring.h"

# ifndef LOCKTO_RM
#  define LOCKTO_RM	300	/* timeout for stale lockfile removal */
# endif /* ! LOCKTO_RM */
# ifndef LOCKTO_GLOB
#  define LOCKTO_GLOB	400	/* global timeout for lockfile creation */
# endif /* ! LOCKTO_GLOB */

# ifdef __STDC__
#  include <stdarg.h>
#  define REALLOC(ptr, size)	realloc(ptr, size)
# else /* __STDC__ */
#  include <varargs.h>
/* define a realloc() which works for NULL pointers */
#  define REALLOC(ptr, size)	(((ptr) == NULL) ? malloc(size) : realloc(ptr, size))
# endif /* __STDC__ */

# if (defined(sun) && defined(__svr4__)) || defined(__SVR4)
#  define USE_LOCKF	1
#  define USE_SETEUID	1
#   define _PATH_MAILDIR	"/var/mail"
# endif /* (defined(sun) && defined(__svr4__)) || defined(__SVR4) */

# ifdef NCR_MP_RAS3
#  define USE_LOCKF	1
#  define HASSNPRINTF	1
#   define _PATH_MAILDIR	"/var/mail"
# endif /* NCR_MP_RAS3 */

# if defined(_AIX)
#  define USE_LOCKF	1
#  define USE_SETEUID	1
# endif /* defined(_AIX) */

# if defined(__hpux)
#  define USE_LOCKF	1
#  define USE_SETRESUID	1
# endif /* defined(__hpux) */

# ifdef DGUX
#  define HASSNPRINTF	1
#  define USE_LOCKF	1
# endif /* DGUX */

# if defined(_CRAY)
#  if !defined(MAXPATHLEN)
#   define MAXPATHLEN PATHSIZE
#  endif /* !defined(MAXPATHLEN) */
#   define _PATH_MAILDIR	"/usr/spool/mail"
# endif /* defined(_CRAY) */

# if defined(NeXT) && !defined(__APPLE__)
#  include <libc.h>
#   define _PATH_MAILDIR	"/usr/spool/mail"
#  define S_IRUSR	S_IREAD
#  define S_IWUSR	S_IWRITE
# endif /* defined(NeXT) && !defined(__APPLE__) */

# if defined(IRIX64) || defined(IRIX5) || defined(IRIX6)
#   include <paths.h>
# endif /* defined(IRIX64) || defined(IRIX5) || defined(IRIX6) */

/*
 * If you don't have flock, you could try using lockf instead.
 */

# ifdef USE_LOCKF
#  define flock(a, b)	lockf(a, b, 0)
#  ifdef LOCK_EX
#   undef LOCK_EX
#  endif /* LOCK_EX */
#  define LOCK_EX	F_LOCK
# endif /* USE_LOCKF */

# ifndef LOCK_EX
#  include <sys/file.h>
# endif /* ! LOCK_EX */

# if defined(BSD4_4) || defined(__GLIBC__)
#   include <paths.h>
#  define _PATH_LOCTMP	"/var/tmp/local.XXXXXX"
# endif /* defined(BSD4_4) || defined(__GLIBC__) */

# ifdef BSD4_4
#  define HAS_ST_GEN	1
# else /* BSD4_4 */
#  ifndef _BSD_VA_LIST_
#   define _BSD_VA_LIST_	va_list
#  endif /* ! _BSD_VA_LIST_ */
# endif /* BSD4_4 */

# if defined(BSD4_4) || defined(linux)
#  define HASSNPRINTF	1
# else /* defined(BSD4_4) || defined(linux) */
#  ifndef ultrix
extern FILE	*fdopen __P((int, const char *));
#  endif /* ! ultrix */
# endif /* defined(BSD4_4) || defined(linux) */

# if SOLARIS >= 20300 || (SOLARIS < 10000 && SOLARIS >= 203)
#  define CONTENTLENGTH	1	/* Needs the Content-Length header */
# endif /* SOLARIS >= 20300 || (SOLARIS < 10000 && SOLARIS >= 203) */

# if SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206)
#  define HASSNPRINTF	1		/* has snprintf starting in 2.6 */
# endif /* SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206) */

# ifdef HPUX11
#  define HASSNPRINTF	1		/* has snprintf starting in 11.X */
# endif /* HPUX11 */

# if _AIX4 >= 40300
#  define HASSNPRINTF	1		/* has snprintf starting in 4.3 */
# endif /* _AIX4 >= 40300 */

# if !HASSNPRINTF && !SFIO
extern int	snprintf __P((char *, size_t, const char *, ...));
#  ifndef _CRAY
extern int	vsnprintf __P((char *, size_t, const char *, ...));
#  endif /* ! _CRAY */
# endif /* !HASSNPRINTF && !SFIO */

/*
**  If you don't have setreuid, and you have saved uids, and you have
**  a seteuid() call that doesn't try to emulate using setuid(), then
**  you can try defining USE_SETEUID.
*/

# ifdef USE_SETEUID
#  define setreuid(r, e)		seteuid(e)
# endif /* USE_SETEUID */

/*
**  And of course on hpux you have setresuid()
*/

# ifdef USE_SETRESUID
#  define setreuid(r, e)		setresuid(-1, e, -1)
# endif /* USE_SETRESUID */

# ifndef _PATH_LOCTMP
#  define _PATH_LOCTMP	"/var/tmp/local.XXXXXX"
# endif /* ! _PATH_LOCTMP */
#  ifndef _PATH_MAILDIR
#   define _PATH_MAILDIR	"/var/spool/mail"
#  endif /* ! _PATH_MAILDIR */

# ifndef S_ISREG
#  define S_ISREG(mode)	(((mode) & _S_IFMT) == S_IFREG)
# endif /* ! S_ISREG */

# ifdef MAILLOCK
#  include <maillock.h>
# endif /* MAILLOCK */

# define U_UID pw->pw_uid
# define U_GID pw->pw_gid

#ifndef INADDRSZ
# define INADDRSZ	4		/* size of an IPv4 address in bytes */
#endif /* ! INADDRSZ */

#ifndef MAILER_DAEMON
# define MAILER_DAEMON	"MAILER-DAEMON"
#endif /* ! MAILER_DAEMON */

#ifdef CONTENTLENGTH
char	ContentHdr[40] = "Content-Length: ";
off_t	HeaderLength;
off_t	BodyLength;
#endif /* CONTENTLENGTH */

bool	EightBitMime = TRUE;		/* advertise 8BITMIME in LMTP */
char	ErrBuf[10240];			/* error buffer */
int	ExitVal = EX_OK;		/* sysexits.h error value. */
bool	HoldErrs = FALSE;		/* Hold errors in ErrBuf */
bool	LMTPMode = FALSE;
bool	BounceQuota = FALSE;		/* permanent error when over quota */
bool	nobiff = FALSE;
bool	nofsync = FALSE;

void	deliver __P((int, char *));
int	e_to_sys __P((int));
void	notifybiff __P((char *));
int	store __P((char *, int, bool *));
void	usage __P((void));
int	lockmbox __P((char *));
void	unlockmbox __P((void));
void	mailerr __P((const char *, const char *, ...));
void	flush_error __P((void));


int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct passwd *pw;
	int ch, fd;
	uid_t uid;
	char *from;
	extern char *optarg;
	extern int optind;


	/* make sure we have some open file descriptors */
	for (fd = 10; fd < 30; fd++)
		(void) close(fd);

	/* use a reasonable umask */
	(void) umask(0077);

# ifdef LOG_MAIL
	openlog("mail.local", 0, LOG_MAIL);
# else /* LOG_MAIL */
	openlog("mail.local", 0);
# endif /* LOG_MAIL */

	from = NULL;
	while ((ch = getopt(argc, argv, "7Bbdf:r:ls")) != -1)
	{
		switch(ch)
		{
		  case '7':		/* Do not advertise 8BITMIME */
			EightBitMime = FALSE;
			break;

		  case 'B':
			nobiff = TRUE;
			break;

		  case 'b':		/* bounce mail when over quota. */
			BounceQuota = TRUE;
			break;

		  case 'd':		/* Backward compatible. */
			break;

		  case 'f':
		  case 'r':		/* Backward compatible. */
			if (from != NULL)
			{
				mailerr(NULL, "Multiple -f options");
				usage();
			}
			from = optarg;
			break;

		  case 'l':
			LMTPMode = TRUE;
			break;

		  case 's':
			nofsync++;
			break;

		  case '?':
		  default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* initialize biff structures */
	if (!nobiff)
		notifybiff(NULL);

	if (LMTPMode)
	{
		extern void dolmtp __P((void));

		if (argc > 0)
		{
			mailerr("421", "Users should not be specified in command line if LMTP required");
			exit(EX_TEMPFAIL);
		}

		dolmtp();
		/* NOTREACHED */
		exit(EX_OK);
	}

	/* Non-LMTP from here on out */
	if (*argv == '\0')
		usage();

	/*
	**  If from not specified, use the name from getlogin() if the
	**  uid matches, otherwise, use the name from the password file
	**  corresponding to the uid.
	*/

	uid = getuid();

	if (from == NULL && ((from = getlogin()) == NULL ||
			     (pw = getpwnam(from)) == NULL ||
			     pw->pw_uid != uid))
		from = (pw = getpwuid(uid)) != NULL ? pw->pw_name : "???";

	/*
	**  There is no way to distinguish the error status of one delivery
	**  from the rest of the deliveries.  So, if we failed hard on one
	**  or more deliveries, but had no failures on any of the others, we
	**  return a hard failure.  If we failed temporarily on one or more
	**  deliveries, we return a temporary failure regardless of the other
	**  failures.  This results in the delivery being reattempted later
	**  at the expense of repeated failures and multiple deliveries.
	*/

	HoldErrs = TRUE;
	fd = store(from, 0, NULL);
	HoldErrs = FALSE;
	if (fd < 0)
	{
		flush_error();
		exit(ExitVal);
	}
	for (; *argv != NULL; ++argv)
		deliver(fd, *argv);
	exit(ExitVal);
	/* NOTREACHED */
	return ExitVal;
}

char *
parseaddr(s, rcpt)
	char *s;
	bool rcpt;
{
	char *p;
	int l;

	if (*s++ != '<')
		return NULL;

	p = s;

	/* at-domain-list */
	while (*p == '@')
	{
		p++;
		while (*p != ',' && *p != ':' && *p != '\0')
			p++;
		if (*p == '\0')
			return NULL;

		/* Skip over , or : */
		p++;
	}

	s = p;

	/* local-part */
	while (*p != '\0' && *p != '@' && *p != '>')
	{
		if (*p == '\\')
		{
			if (*++p == '\0')
				return NULL;
		}
		else if (*p == '\"')
		{
			p++;
			while (*p != '\0' && *p != '\"')
			{
				if (*p == '\\')
				{
					if (*++p == '\0')
						return NULL;
				}
				p++;
			}
			if (*p == '\0' || *(p + 1) == '\0')
				return NULL;
		}
		/* +detail ? */
		if (*p == '+' && rcpt)
			*p = '\0';
		p++;
	}

	/* @domain */
	if (*p == '@')
	{
		if (rcpt)
			*p++ = '\0';
		while (*p != '\0' && *p != '>')
			p++;
	}

	if (*p != '>')
		return NULL;
	else
		*p = '\0';
	p++;

	if (*p != '\0' && *p != ' ')
		return NULL;

	if (*s == '\0')
		s = MAILER_DAEMON;

	l = strlen(s) + 1;
	p = malloc(l);
	if (p == NULL)
	{
		mailerr("421 4.3.0", "Memory exhausted");
		exit(EX_TEMPFAIL);
	}

	(void) strlcpy(p, s, l);
	return p;
}

char *
process_recipient(addr)
	char *addr;
{
	if (getpwnam(addr) == NULL)
		return "550 5.1.1 User unknown";
	return NULL;
}

#define RCPT_GROW	30

void
dolmtp()
{
	char *return_path = NULL;
	char **rcpt_addr = NULL;
	int rcpt_num = 0;
	int rcpt_alloc = 0;
	bool gotlhlo = FALSE;
	char *err;
	int msgfd;
	char *p;
	int i;
	char myhostname[1024];
	char buf[4096];

	memset(myhostname, '\0', sizeof myhostname);
	(void) gethostname(myhostname, sizeof myhostname - 1);
	if (myhostname[0] == '\0')
		strlcpy(myhostname, "localhost", sizeof myhostname);

	printf("220 %s LMTP ready\r\n", myhostname);
	for (;;)
	{
		(void) fflush(stdout);
		if (fgets(buf, sizeof(buf) - 1, stdin) == NULL)
			exit(EX_OK);
		p = buf + strlen(buf) - 1;
		if (p >= buf && *p == '\n')
			*p-- = '\0';
		if (p >= buf && *p == '\r')
			*p-- = '\0';

		switch (buf[0])
		{
		  case 'd':
		  case 'D':
			if (strcasecmp(buf, "data") == 0)
			{
				bool inbody = FALSE;

				if (rcpt_num == 0)
				{
					mailerr("503 5.5.1", "No recipients");
					continue;
				}
				HoldErrs = TRUE;
				msgfd = store(return_path, rcpt_num, &inbody);
				HoldErrs = FALSE;
				if (msgfd < 0 && !inbody)
				{
					flush_error();
					continue;
				}

				for (i = 0; i < rcpt_num; i++)
				{
					if (msgfd < 0)
					{
						/* print error for rcpt */
						flush_error();
						continue;
					}
					p = strchr(rcpt_addr[i], '+');
					if (p != NULL)
						*p = '\0';
					deliver(msgfd, rcpt_addr[i]);
				}
				if (msgfd >= 0)
					(void) close(msgfd);
				goto rset;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'l':
		  case 'L':
			if (strncasecmp(buf, "lhlo ", 5) == 0)
			{
				/* check for duplicate per RFC 1651 4.2 */
				if (gotlhlo)
				{
					mailerr("503", "%s Duplicate LHLO",
					       myhostname);
					continue;
				}
				gotlhlo = TRUE;
				printf("250-%s\r\n", myhostname);
				if (EightBitMime)
					printf("250-8BITMIME\r\n");
				printf("250-ENHANCEDSTATUSCODES\r\n");
				printf("250 PIPELINING\r\n");
				continue;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'm':
		  case 'M':
			if (strncasecmp(buf, "mail ", 5) == 0)
			{
				if (return_path != NULL)
				{
					mailerr("503 5.5.1",
						"Nested MAIL command");
					continue;
				}
				if (strncasecmp(buf+5, "from:", 5) != 0 ||
				    ((return_path = parseaddr(buf + 10,
							      FALSE)) == NULL))
				{
					mailerr("501 5.5.4",
						"Syntax error in parameters");
					continue;
				}
				printf("250 2.5.0 Ok\r\n");
				continue;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'n':
		  case 'N':
			if (strcasecmp(buf, "noop") == 0)
			{
				printf("250 2.0.0 Ok\r\n");
				continue;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'q':
		  case 'Q':
			if (strcasecmp(buf, "quit") == 0)
			{
				printf("221 2.0.0 Bye\r\n");
				exit(EX_OK);
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'r':
		  case 'R':
			if (strncasecmp(buf, "rcpt ", 5) == 0)
			{
				if (return_path == NULL)
				{
					mailerr("503 5.5.1",
						"Need MAIL command");
					continue;
				}
				if (rcpt_num >= rcpt_alloc)
				{
					rcpt_alloc += RCPT_GROW;
					rcpt_addr = (char **)
						REALLOC((char *) rcpt_addr,
							rcpt_alloc *
							sizeof(char **));
					if (rcpt_addr == NULL)
					{
						mailerr("421 4.3.0",
							"Memory exhausted");
						exit(EX_TEMPFAIL);
					}
				}
				if (strncasecmp(buf + 5, "to:", 3) != 0 ||
				    ((rcpt_addr[rcpt_num] = parseaddr(buf + 8,
								      TRUE)) == NULL))
				{
					mailerr("501 5.5.4",
						"Syntax error in parameters");
					continue;
				}
				err = process_recipient(rcpt_addr[rcpt_num]);
				if (err != NULL)
				{
					mailerr(NULL, "%s", err);
					continue;
				}
				rcpt_num++;
				printf("250 2.1.5 Ok\r\n");
				continue;
			}
			else if (strcasecmp(buf, "rset") == 0)
			{
				printf("250 2.0.0 Ok\r\n");

rset:
				while (rcpt_num > 0)
					free(rcpt_addr[--rcpt_num]);
				if (return_path != NULL)
					free(return_path);
				return_path = NULL;
				continue;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  case 'v':
		  case 'V':
			if (strncasecmp(buf, "vrfy ", 5) == 0)
			{
				printf("252 2.3.3 Try RCPT to attempt delivery\r\n");
				continue;
			}
			goto syntaxerr;
			/* NOTREACHED */
			break;

		  default:
  syntaxerr:
			mailerr("500 5.5.2", "Syntax error");
			continue;
			/* NOTREACHED */
			break;
		}
	}
}

int
store(from, lmtprcpts, inbody)
	char *from;
	int lmtprcpts;
	bool *inbody;
{
	FILE *fp = NULL;
	time_t tval;
	bool eline;
	bool fullline = TRUE;	/* current line is terminated */
	bool prevfl;		/* previous line was terminated */
	char line[2048];
	int fd;
	char tmpbuf[sizeof _PATH_LOCTMP + 1];

	if (inbody != NULL)
		*inbody = FALSE;

	(void) umask(0077);
	(void) strlcpy(tmpbuf, _PATH_LOCTMP, sizeof tmpbuf);
	if ((fd = mkstemp(tmpbuf)) < 0 || (fp = fdopen(fd, "w+")) == NULL)
	{
		mailerr("451 4.3.0", "Unable to open temporary file");
		return -1;
	}
	(void) unlink(tmpbuf);

	if (LMTPMode)
	{
		printf("354 Go ahead\r\n");
		(void) fflush(stdout);
	}
	if (inbody != NULL)
		*inbody = TRUE;

	(void) time(&tval);
	(void) fprintf(fp, "From %s %s", from, ctime(&tval));

#ifdef CONTENTLENGTH
	HeaderLength = 0;
	BodyLength = -1;
#endif /* CONTENTLENGTH */

	line[0] = '\0';
	eline = TRUE;
	while (fgets(line, sizeof(line), stdin) != (char *) NULL)
	{
		size_t line_len = 0;
		int peek;

		prevfl = fullline;	/* preserve state of previous line */
		while (line[line_len] != '\n' && line_len < sizeof(line) - 2)
			line_len++;
		line_len++;

		/* Check for dot-stuffing */
		if (prevfl && LMTPMode && line[0] == '.')
		{
			if (line[1] == '\n' ||
			    (line[1] == '\r' && line[2] == '\n'))
				goto lmtpdot;
			memcpy(line, line + 1, line_len);
			line_len--;
		}

		/* Check to see if we have the full line from fgets() */
		fullline = FALSE;
		if (line_len > 0)
		{
			if (line[line_len - 1] == '\n')
			{
				if (line_len >= 2 &&
				    line[line_len - 2] == '\r')
				{
					line[line_len - 2] = '\n';
					line[line_len - 1] = '\0';
					line_len--;
				}
				fullline = TRUE;
			}
			else if (line[line_len - 1] == '\r')
			{
				/* Did we just miss the CRLF? */
				peek = fgetc(stdin);
				if (peek == '\n')
				{
					line[line_len - 1] = '\n';
					fullline = TRUE;
				}
				else
					(void) ungetc(peek, stdin);
			}
		}
		else
			fullline = TRUE;

#ifdef CONTENTLENGTH
		if (prevfl && line[0] == '\n' && HeaderLength == 0)
		{
			eline = FALSE;
			if (fp != NULL)
				HeaderLength = ftell(fp);
			if (HeaderLength <= 0)
			{
				/*
				**  shouldn't happen, unless ftell() is
				**  badly broken
				*/

				HeaderLength = -1;
			}
		}
#else /* CONTENTLENGTH */
		if (prevfl && line[0] == '\n')
			eline = TRUE;
#endif /* CONTENTLENGTH */
		else
		{
			if (eline && line[0] == 'F' &&
			    fp != NULL &&
			    !memcmp(line, "From ", 5))
				(void) putc('>', fp);
			eline = FALSE;
#ifdef CONTENTLENGTH
			/* discard existing "Content-Length:" headers */
			if (prevfl && HeaderLength == 0 &&
			    (line[0] == 'C' || line[0] == 'c') &&
			    strncasecmp(line, ContentHdr, 15) == 0)
			{
				/*
				**  be paranoid: clear the line
				**  so no "wrong matches" may occur later
				*/
				line[0] = '\0';
				continue;
			}
#endif /* CONTENTLENGTH */

		}
		if (fp != NULL)
		{
			(void) fwrite(line, sizeof(char), line_len, fp);
			if (ferror(fp))
			{
				mailerr("451 4.3.0",
					"Temporary file write error");
				(void) fclose(fp);
				fp = NULL;
				continue;
			}
		}
	}

	/* check if an error occurred */
	if (fp == NULL)
		return -1;

	if (LMTPMode)
	{
		/* Got a premature EOF -- toss message and exit */
		exit(EX_OK);
	}

	/* If message not newline terminated, need an extra. */
	if (fp != NULL && strchr(line, '\n') == NULL)
		(void) putc('\n', fp);

  lmtpdot:

#ifdef CONTENTLENGTH
	if (fp != NULL)
		BodyLength = ftell(fp);
	if (HeaderLength == 0 && BodyLength > 0)	/* empty body */
	{
		HeaderLength = BodyLength;
		BodyLength = 0;
	}
	else
		BodyLength = BodyLength - HeaderLength - 1 ;

	if (HeaderLength > 0 && BodyLength >= 0)
	{
		extern char *quad_to_string();

		if (sizeof BodyLength > sizeof(long))
			snprintf(line, sizeof line, "%s\n",
				 quad_to_string(BodyLength));
		else
			snprintf(line, sizeof line, "%ld\n",
				 (long) BodyLength);
		strlcpy(&ContentHdr[16], line, sizeof(ContentHdr) - 16);
	}
	else
		BodyLength = -1;	/* Something is wrong here */
#endif /* CONTENTLENGTH */

	/* Output a newline; note, empty messages are allowed. */
	if (fp != NULL)
		(void) putc('\n', fp);

	if (fp == NULL || fflush(fp) == EOF || ferror(fp) != 0)
	{
		mailerr("451 4.3.0", "Temporary file write error");
		if (fp != NULL)
			(void) fclose(fp);
		return -1;
	}
	return fd;
}

void
deliver(fd, name)
	int fd;
	char *name;
{
	struct stat fsb;
	struct stat sb;
	struct passwd *pw;
	char path[MAXPATHLEN];
	int mbfd = -1, nr = 0, nw, off;
	char *p;
	char *errcode;
	off_t curoff;
#ifdef CONTENTLENGTH
	off_t headerbytes;
	int readamount;
#endif /* CONTENTLENGTH */
	char biffmsg[100], buf[8*1024];
	extern char *quad_to_string();


	/*
	**  Disallow delivery to unknown names -- special mailboxes can be
	**  handled in the sendmail aliases file.
	*/

	if ((pw = getpwnam(name)) == NULL)
	{
		if (ExitVal == EX_TEMPFAIL)
			errcode = "451 4.3.0";
		else
		{
			ExitVal = EX_UNAVAILABLE;
			errcode = "550 5.1.1";
		}
		mailerr(errcode, "Unknown name: %s", name);
		return;
	}
	endpwent();

	/*
	**  Keep name reasonably short to avoid buffer overruns.
	**	This isn't necessary on BSD because of the proper
	**	definition of snprintf(), but it can cause problems
	**	on other systems.
	**  Also, clear out any bogus characters.
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


	(void) snprintf(path, sizeof(path), "%s/%s", _PATH_MAILDIR, name);


	/*
	**  If the mailbox is linked or a symlink, fail.  There's an obvious
	**  race here, that the file was replaced with a symbolic link after
	**  the lstat returned, but before the open.  We attempt to detect
	**  this by comparing the original stat information and information
	**  returned by an fstat of the file descriptor returned by the open.
	**
	**  NB: this is a symptom of a larger problem, that the mail spooling
	**  directory is writeable by the wrong users.  If that directory is
	**  writeable, system security is compromised for other reasons, and
	**  it cannot be fixed here.
	**
	**  If we created the mailbox, set the owner/group.  If that fails,
	**  just return.  Another process may have already opened it, so we
	**  can't unlink it.  Historically, binmail set the owner/group at
	**  each mail delivery.  We no longer do this, assuming that if the
	**  ownership or permissions were changed there was a reason.
	**
	**  XXX
	**  open(2) should support flock'ing the file.
	*/

tryagain:
#ifdef MAILLOCK
	p = name;
#else /* MAILLOCK */
	p = path;
#endif /* MAILLOCK */
	if ((off = lockmbox(p)) != 0)
	{
		if (off == EX_TEMPFAIL || e_to_sys(off) == EX_TEMPFAIL)
		{
			ExitVal = EX_TEMPFAIL;
			errcode = "451 4.3.0";
		}
		else
			errcode = "551 5.3.0";

		mailerr(errcode, "lockmailbox %s failed; error code %d %s",
			p, off, errno > 0 ? errstring(errno) : "");
		return;
	}

	if (lstat(path, &sb) < 0)
	{
		int save_errno;
		int mode = S_IRUSR|S_IWUSR;
		gid_t gid = U_GID;

#ifdef MAILGID
		(void) umask(0007);
		gid = MAILGID;
		mode |= S_IRGRP|S_IWGRP;
#endif /* MAILGID */

		mbfd = open(path, O_APPEND|O_CREAT|O_EXCL|O_WRONLY|EXTRA_MODE,
			    mode);
		save_errno = errno;

		if (lstat(path, &sb) < 0)
		{
			ExitVal = EX_CANTCREAT;
			mailerr("550 5.2.0",
				"%s: lstat: file changed after open", path);
			goto err1;
		}
		if (mbfd < 0)
		{
			if (save_errno == EEXIST)
				goto tryagain;

			/* open failed, don't try again */
			mailerr("450 4.2.0", "%s: %s", path,
				errstring(save_errno));
			goto err0;
		}
		else if (fchown(mbfd, U_UID, gid) < 0)
		{
			mailerr("451 4.3.0", "chown %u.%u: %s",
				U_UID, gid, name);
			goto err1;
		}
		else
		{
			/*
			**  open() was successful, now close it so can
			**  be opened as the right owner again.
			**  Paranoia: reset mbdf since the file descriptor
			**  is no longer valid; better safe than sorry.
			*/

			sb.st_uid = U_UID;
			(void) close(mbfd);
			mbfd = -1;
		}
	}
	else if (sb.st_nlink != 1 || !S_ISREG(sb.st_mode))
	{
		mailerr("550 5.2.0", "%s: irregular file", path);
		goto err0;
	}
	else if (sb.st_uid != U_UID)
	{
		ExitVal = EX_CANTCREAT;
		mailerr("550 5.2.0", "%s: wrong ownership (%d)",
			path, sb.st_uid);
		goto err0;
	}

	/* change UID for quota checks */
	if (setreuid(0, U_UID) < 0)
	{
		mailerr("450 4.2.0", "setreuid(0, %d): %s (r=%d, e=%d)",
			U_UID, errstring(errno), getuid(), geteuid());
		goto err1;
	}
#ifdef DEBUG
	fprintf(stderr, "new euid = %d\n", geteuid());
#endif /* DEBUG */
	mbfd = open(path, O_APPEND|O_WRONLY|EXTRA_MODE, 0);
	if (mbfd < 0)
	{
		mailerr("450 4.2.0", "%s: %s", path, errstring(errno));
		goto err0;
	}
	else if (fstat(mbfd, &fsb) < 0 ||
		 fsb.st_nlink != 1 ||
		 sb.st_nlink != 1 ||
		 !S_ISREG(fsb.st_mode) ||
		 sb.st_dev != fsb.st_dev ||
		 sb.st_ino != fsb.st_ino ||
# if HAS_ST_GEN && 0		/* AFS returns random values for st_gen */
		 sb.st_gen != fsb.st_gen ||
# endif /* HAS_ST_GEN && 0 */
		 sb.st_uid != fsb.st_uid)
	{
		ExitVal = EX_TEMPFAIL;
		mailerr("550 5.2.0", "%s: fstat: file changed after open",
			path);
		goto err1;
	}


	/* Wait until we can get a lock on the file. */
	if (flock(mbfd, LOCK_EX) < 0)
	{
		mailerr("450 4.2.0", "%s: %s", path, errstring(errno));
		goto err1;
	}

	if (!nobiff)
	{
		/* Get the starting offset of the new message for biff. */
		curoff = lseek(mbfd, (off_t) 0, SEEK_END);
		if (sizeof curoff > sizeof(long))
			(void) snprintf(biffmsg, sizeof(biffmsg), "%s@%s\n",
					name, quad_to_string(curoff));
		else
			(void) snprintf(biffmsg, sizeof(biffmsg), "%s@%ld\n",
					name, (long) curoff);
	}

	/* Copy the message into the file. */
	if (lseek(fd, (off_t) 0, SEEK_SET) == (off_t) -1)
	{
		mailerr("450 4.2.0", "Temporary file: %s",
			errstring(errno));
		goto err1;
	}
#ifdef DEBUG
	fprintf(stderr, "before writing: euid = %d\n", geteuid());
#endif /* DEBUG */
#ifdef CONTENTLENGTH
	headerbytes = (BodyLength >= 0) ? HeaderLength : -1 ;
	for (;;)
	{
		if (headerbytes == 0)
		{
			snprintf(buf, sizeof buf, "%s", ContentHdr);
			nr = strlen(buf);
			headerbytes = -1;
			readamount = 0;
		}
		else if (headerbytes > sizeof(buf) || headerbytes < 0)
			readamount = sizeof(buf);
		else
			readamount = headerbytes;
		if (readamount != 0)
			nr = read(fd, buf, readamount);
		if (nr <= 0)
			break;
		if (headerbytes > 0)
			headerbytes -= nr ;

#else /* CONTENTLENGTH */
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
	{
#endif /* CONTENTLENGTH */
		for (off = 0; off < nr; off += nw)
		{
			if ((nw = write(mbfd, buf + off, nr - off)) < 0)
			{
				errcode = "450 4.2.0";
#ifdef EDQUOT
				if (errno == EDQUOT && BounceQuota)
					errcode = "552 5.2.2";
#endif /* EDQUOT */
				mailerr(errcode, "%s: %s",
					path, errstring(errno));
				goto err3;
			}
		}
	}
	if (nr < 0)
	{
		mailerr("450 4.2.0", "Temporary file: %s",
			errstring(errno));
		goto err3;
	}

	/* Flush to disk, don't wait for update. */
	if (!nofsync && fsync(mbfd) < 0)
	{
		mailerr("450 4.2.0", "%s: %s", path, errstring(errno));
err3:
		(void) setreuid(0, 0);
#ifdef DEBUG
		fprintf(stderr, "reset euid = %d\n", geteuid());
#endif /* DEBUG */
		(void) ftruncate(mbfd, curoff);
err1:		if (mbfd >= 0)
			(void) close(mbfd);
err0:		unlockmbox();
		return;
	}

	/* Close and check -- NFS doesn't write until the close. */
	if (close(mbfd))
	{
		errcode = "450 4.2.0";
#ifdef EDQUOT
		if (errno == EDQUOT && BounceQuota)
			errcode = "552 5.2.2";
#endif /* EDQUOT */
		mailerr(errcode, "%s: %s", path, errstring(errno));
		(void) truncate(path, curoff);
	}
	else if (!nobiff)
		notifybiff(biffmsg);

	if (setreuid(0, 0) < 0)
	{
		mailerr("450 4.2.0", "setreuid(0, 0): %s",
			errstring(errno));
		goto err0;
	}
#ifdef DEBUG
	fprintf(stderr, "reset euid = %d\n", geteuid());
#endif /* DEBUG */
	unlockmbox();
	if (LMTPMode)
		printf("250 2.1.5 %s Ok\r\n", name);
}

/*
**  user.lock files are necessary for compatibility with other
**  systems, e.g., when the mail spool file is NFS exported.
**  Alas, mailbox locking is more than just a local matter.
**  EPA 11/94.
*/

bool	Locked = FALSE;

#ifdef MAILLOCK
int
lockmbox(name)
	char *name;
{
	int r = 0;

	if (Locked)
		return 0;
	if ((r = maillock(name, 15)) == L_SUCCESS)
	{
		Locked = TRUE;
		return 0;
	}
	switch (r)
	{
	  case L_TMPLOCK:	/* Can't create tmp file */
	  case L_TMPWRITE:	/* Can't write pid into lockfile */
	  case L_MAXTRYS:	/* Failed after retrycnt attempts */
		errno = 0;
		r = EX_TEMPFAIL;
		break;
	  case L_ERROR:		/* Check errno for reason */
		r = errno;
		break;
	  default:		/* other permanent errors */
		errno = 0;
		r = EX_UNAVAILABLE;
		break;
	}
	return r;
}

void
unlockmbox()
{
	if (Locked)
		mailunlock();
	Locked = FALSE;
}
#else /* MAILLOCK */

char	LockName[MAXPATHLEN];

int
lockmbox(path)
	char *path;
{
	int statfailed = 0;
	time_t start;

	if (Locked)
		return 0;
	if (strlen(path) + 6 > sizeof LockName)
		return EX_SOFTWARE;
	(void) snprintf(LockName, sizeof LockName, "%s.lock", path);
	(void) time(&start);
	for (; ; sleep(5))
	{
		int fd;
		struct stat st;
		time_t now;

		/* global timeout */
		(void) time(&now);
		if (now > start + LOCKTO_GLOB)
		{
			errno = 0;
			return EX_TEMPFAIL;
		}
		fd = open(LockName, O_WRONLY|O_EXCL|O_CREAT, 0);
		if (fd >= 0)
		{
			/* defeat lock checking programs which test pid */
			(void) write(fd, "0", 2);
			Locked = TRUE;
			(void) close(fd);
			return 0;
		}
		if (stat(LockName, &st) < 0)
		{
			if (statfailed++ > 5)
			{
				errno = 0;
				return EX_TEMPFAIL;
			}
			continue;
		}
		statfailed = 0;
		(void) time(&now);
		if (now < st.st_ctime + LOCKTO_RM)
			continue;

		/* try to remove stale lockfile */
		if (unlink(LockName) < 0)
			return errno;
	}
}

void
unlockmbox()
{
	if (!Locked)
		return;
	(void) unlink(LockName);
	Locked = FALSE;
}
#endif /* MAILLOCK */

void
notifybiff(msg)
	char *msg;
{
	static bool initialized = FALSE;
	static int f = -1;
	struct hostent *hp;
	struct servent *sp;
	int len;
	static struct sockaddr_in addr;

	if (!initialized)
	{
		initialized = TRUE;

		/* Be silent if biff service not available. */
		if ((sp = getservbyname("biff", "udp")) == NULL ||
		    (hp = gethostbyname("localhost")) == NULL ||
		    hp->h_length != INADDRSZ)
			return;

		addr.sin_family = hp->h_addrtype;
		memcpy(&addr.sin_addr, hp->h_addr, INADDRSZ);
		addr.sin_port = sp->s_port;
	}

	/* No message, just return */
	if (msg == NULL)
		return;

	/* Couldn't initialize addr struct */
	if (addr.sin_family == AF_UNSPEC)
		return;

	if (f < 0 && (f = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return;
	len = strlen(msg) + 1;
	(void) sendto(f, msg, len, 0, (struct sockaddr *) &addr, sizeof(addr));
}

void
usage()
{
	ExitVal = EX_USAGE;
	mailerr(NULL, "usage: mail.local [-7] [-B] [-b] [-l] [-f from] [-s] user ...");
	exit(ExitVal);
}

void
#ifdef __STDC__
mailerr(const char *hdr, const char *fmt, ...)
#else /* __STDC__ */
mailerr(hdr, fmt, va_alist)
	const char *hdr;
	const char *fmt;
	va_dcl
#endif /* __STDC__ */
{
	size_t len = 0;
	va_list ap;

	(void) e_to_sys(errno);

#ifdef __STDC__
	va_start(ap, fmt);
#else /* __STDC__ */
	va_start(ap);
#endif /* __STDC__ */

	if (LMTPMode)
	{
		if (hdr != NULL)
		{
			snprintf(ErrBuf, sizeof ErrBuf, "%s ", hdr);
			len = strlen(ErrBuf);
		}
	}
	(void) vsnprintf(&ErrBuf[len], sizeof ErrBuf - len, fmt, ap);

	if (!HoldErrs)
		flush_error();

	/* Log the message to syslog. */
	if (!LMTPMode)
		syslog(LOG_ERR, "%s", ErrBuf);
}

void
flush_error()
{
	if (LMTPMode)
		printf("%s\r\n", ErrBuf);
	else
	{
		if (ExitVal != EX_USAGE)
			(void) fprintf(stderr, "mail.local: ");
		fprintf(stderr, "%s\n", ErrBuf);
	}
}

/*
 * e_to_sys --
 *	Guess which errno's are temporary.  Gag me.
 */

int
e_to_sys(num)
	int num;
{
	/* Temporary failures override hard errors. */
	if (ExitVal == EX_TEMPFAIL)
		return ExitVal;

	switch (num)		/* Hopefully temporary errors. */
	{
#ifdef EDQUOT
	  case EDQUOT:		/* Disc quota exceeded */
		if (BounceQuota)
		{
			ExitVal = EX_UNAVAILABLE;
			break;
		}
		/* FALLTHROUGH */
#endif /* EDQUOT */
#ifdef EAGAIN
	  case EAGAIN:		/* Resource temporarily unavailable */
#endif /* EAGAIN */
#ifdef EBUSY
	  case EBUSY:		/* Device busy */
#endif /* EBUSY */
#ifdef EPROCLIM
	  case EPROCLIM:	/* Too many processes */
#endif /* EPROCLIM */
#ifdef EUSERS
	  case EUSERS:		/* Too many users */
#endif /* EUSERS */
#ifdef ECONNABORTED
	  case ECONNABORTED:	/* Software caused connection abort */
#endif /* ECONNABORTED */
#ifdef ECONNREFUSED
	  case ECONNREFUSED:	/* Connection refused */
#endif /* ECONNREFUSED */
#ifdef ECONNRESET
	  case ECONNRESET:	/* Connection reset by peer */
#endif /* ECONNRESET */
#ifdef EDEADLK
	  case EDEADLK:		/* Resource deadlock avoided */
#endif /* EDEADLK */
#ifdef EFBIG
	  case EFBIG:		/* File too large */
#endif /* EFBIG */
#ifdef EHOSTDOWN
	  case EHOSTDOWN:	/* Host is down */
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
	  case EHOSTUNREACH:	/* No route to host */
#endif /* EHOSTUNREACH */
#ifdef EMFILE
	  case EMFILE:		/* Too many open files */
#endif /* EMFILE */
#ifdef ENETDOWN
	  case ENETDOWN:	/* Network is down */
#endif /* ENETDOWN */
#ifdef ENETRESET
	  case ENETRESET:	/* Network dropped connection on reset */
#endif /* ENETRESET */
#ifdef ENETUNREACH
	  case ENETUNREACH:	/* Network is unreachable */
#endif /* ENETUNREACH */
#ifdef ENFILE
	  case ENFILE:		/* Too many open files in system */
#endif /* ENFILE */
#ifdef ENOBUFS
	  case ENOBUFS:		/* No buffer space available */
#endif /* ENOBUFS */
#ifdef ENOMEM
	  case ENOMEM:		/* Cannot allocate memory */
#endif /* ENOMEM */
#ifdef ENOSPC
	  case ENOSPC:		/* No space left on device */
#endif /* ENOSPC */
#ifdef EROFS
	  case EROFS:		/* Read-only file system */
#endif /* EROFS */
#ifdef ESTALE
	  case ESTALE:		/* Stale NFS file handle */
#endif /* ESTALE */
#ifdef ETIMEDOUT
	  case ETIMEDOUT:	/* Connection timed out */
#endif /* ETIMEDOUT */
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN && EWOULDBLOCK != EDEADLK
	  case EWOULDBLOCK:	/* Operation would block. */
#endif /* defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN && EWOULDBLOCK != EDEADLK */
		ExitVal = EX_TEMPFAIL;
		break;

	  default:
		ExitVal = EX_UNAVAILABLE;
		break;
	}
	return ExitVal;
}

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

# if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mktemp.c	8.1 (Berkeley) 6/4/93";
# endif /* defined(LIBC_SCCS) && !defined(lint) */

# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <errno.h>
# include <stdio.h>
# include <ctype.h>

static int _gettemp();

mkstemp(path)
	char *path;
{
	int fd;

	return (_gettemp(path, &fd) ? fd : -1);
}

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
	while (*--trv == 'X')
	{
		*trv = (pid % 10) + '0';
		pid /= 10;
	}

	/*
	 * check the target directory; if you have six X's and it
	 * doesn't exist this runs for a *very* long time.
	 */
	for (start = trv + 1;; --trv)
	{
		if (trv <= path)
			break;
		if (*trv == '/')
		{
			*trv = '\0';
			if (stat(path, &sbuf) < 0)
				return(0);
			if (!S_ISDIR(sbuf.st_mode))
			{
				errno = ENOTDIR;
				return(0);
			}
			*trv = '/';
			break;
		}
	}

	for (;;)
	{
		if (doopen)
		{
			if ((*doopen = open(path, O_CREAT|O_EXCL|O_RDWR,
					    0600)) >= 0)
				return(1);
			if (errno != EEXIST)
				return(0);
		}
		else if (stat(path, &sbuf) < 0)
			return(errno == ENOENT ? 1 : 0);

		/* tricky little algorithm for backward compatibility */
		for (trv = start;;)
		{
			if (!*trv)
				return(0);
			if (*trv == 'z')
				*trv++ = 'a';
			else
			{
				if (isascii(*trv) && isdigit(*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/* NOTREACHED */
}
#endif /* defined(ultrix) || defined(_CRAY) */
