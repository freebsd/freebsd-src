/*
 * Copyright (c) 1983, 1988, 1993, 1994
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
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 * Extension to log by program name as well as facility and priority
 *   by Peter da Silva.
 * -u and -v by Harlan Stenn.
 * Priority comparison code by Harlan Stenn.
 */

#define	MAXLINE		1024		/* maximum line length */
#define	MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timed out passed to ttymsg */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syslimits.h>
#include <paths.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <utmp.h>
#include "pathnames.h"

#define SYSLOG_NAMES
#include <sys/syslog.h>

#ifdef NI_WITHSCOPEID
static const int withscopeid = NI_WITHSCOPEID;
#else
static const int withscopeid = 0;
#endif

const char	*ConfFile = _PATH_LOGCONF;
const char	*PidFile = _PATH_LOGPID;
const char	ctty[] = _PATH_CONSOLE;

#define	dprintf		if (Debug) printf

#define MAXUNAMES	20	/* maximum number of user names */

#define MAXFUNIX       20

int nfunix = 1;
char *funixn[MAXFUNIX] = { _PATH_LOG };
int funix[MAXFUNIX];

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */
#define ISKERNEL	0x010	/* kernel generated message */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	char	*f_host;		/* host from which to recd. */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	u_char	f_pcmp[LOG_NFACILITIES+1];	/* compare priority */
#define PRI_LT	0x1
#define PRI_EQ	0x2
#define PRI_GT	0x4
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			struct addrinfo *f_addr;

		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
		struct {
			char	f_pname[MAXPATHLEN];
			pid_t	f_pid;
		} f_pipe;
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	int	f_repeatcount;			/* number of "repeated" msgs */
};

/*
 * Queue of about-to-be dead processes we should watch out for.
 */

TAILQ_HEAD(stailhead, deadq_entry) deadq_head;
struct stailhead *deadq_headp;

struct deadq_entry {
	pid_t				dq_pid;
	int				dq_timeout;
	TAILQ_ENTRY(deadq_entry)	dq_entries;
};

/*
 * The timeout to apply to processes waiting on the dead queue.  Unit
 * of measure is `mark intervals', i.e. 20 minutes by default.
 * Processes on the dead queue will be terminated after that time.
 */

#define DQ_TIMO_INIT	2

typedef struct deadq_entry *dq_t;


/*
 * Struct to hold records of network addresses that are allowed to log
 * to us.
 */
struct allowedpeer {
	int isnumeric;
	u_short port;
	union {
		struct {
			struct sockaddr_storage addr;
			struct sockaddr_storage mask;
		} numeric;
		char *name;
	} u;
#define a_addr u.numeric.addr
#define a_mask u.numeric.mask
#define a_name u.name
};


/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_PIPE		7		/* pipe to program */

char	*TypeNames[8] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"PIPE"
};

struct	filed *Files;
struct	filed consfile;

int	Debug;			/* debug flag */
int	resolve = 1;		/* resolve hostname */
char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	*finet = NULL;		/* Internet datagram socket */
int	fklog = -1;		/* /dev/klog */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 0;		/* when true, receive only unix domain socks */
#ifdef INET6
int	family = PF_UNSPEC;	/* protocol family (IPv4, IPv6 or both) */
#else
int	family = PF_INET;	/* protocol family (IPv4 only) */
#endif
int	send_to_all = 0;	/* send message to all IPv4/IPv6 addresses */

char	bootfile[MAXLINE+1];	/* booted kernel file */

struct allowedpeer *AllowedPeers;
int	NumAllowed = 0;		/* # of AllowedPeer entries */

int	UniquePriority = 0;	/* Only log specified priority? */
int	LogFacPri = 0;		/* Put facility and priority in log message: */
				/* 0=no, 1=numeric, 2=names */
int	KeepKernFac = 0;	/* Keep remotely logged kernel facility */

int	allowaddr __P((char *));
void	cfline __P((char *, struct filed *, char *, char *));
char   *cvthname __P((struct sockaddr *));
void	deadq_enter __P((pid_t, const char *));
int	deadq_remove __P((pid_t));
int	decode __P((const char *, CODE *));
void	die __P((int));
void	domark __P((int));
void	fprintlog __P((struct filed *, int, char *));
int*	socksetup __P((int));
void	init __P((int));
void	logerror __P((const char *));
void	logmsg __P((int, char *, char *, int));
void	log_deadchild __P((pid_t, int, const char *));
void	printline __P((char *, char *));
void	printsys __P((char *));
int	p_open __P((char *, pid_t *));
void	readklog __P((void));
void	reapchild __P((int));
char   *ttymsg __P((struct iovec *, int, char *, int));
static void	usage __P((void));
int	validate __P((struct sockaddr *, const char *));
static void	unmapped __P((struct sockaddr *));
void	wallmsg __P((struct filed *, struct iovec *));
int	waitdaemon __P((int, int, int));
void	timedout __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i, l;
	struct sockaddr_un sunx, fromunix;
	struct sockaddr_storage frominet;
	FILE *fp;
	char *p, *hname, line[MAXLINE + 1];
	struct timeval tv, *tvp;
	struct sigaction sact;
	sigset_t mask;
	pid_t ppid = 1;
	socklen_t len;

	while ((ch = getopt(argc, argv, "46Aa:df:kl:m:np:P:suv")) != -1)
		switch (ch) {
		case '4':
			family = PF_INET;
			break;
#ifdef INET6
		case '6':
			family = PF_INET6;
			break;
#endif
		case 'A':
			send_to_all++;
			break;
		case 'a':		/* allow specific network addresses only */
			if (allowaddr(optarg) == -1)
				usage();
			break;
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'k':		/* keep remote kern fac */
			KeepKernFac = 1;
			break;
		case 'l':
			if (nfunix < MAXFUNIX)
				funixn[nfunix++] = optarg;
			else
				warnx("out of descriptors, ignoring %s",
					optarg);
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'n':
			resolve = 0;
			break;
		case 'p':		/* path */
			funixn[0] = optarg;
			break;
		case 'P':		/* path for alt. PID */
			PidFile = optarg;
			break;
		case 's':		/* no network mode */
			SecureMode++;
			break;
		case 'u':		/* only log specified priority */
		        UniquePriority++;
			break;
		case 'v':		/* log facility and priority */
		  	LogFacPri++;
			break;
		case '?':
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	if (!Debug) {
		ppid = waitdaemon(0, 0, 30);
		if (ppid < 0)
			err(1, "could not become daemon");
	} else
		setlinebuf(stdout);

	if (NumAllowed)
		endservent();

	consfile.f_type = F_CONSOLE;
	(void)strcpy(consfile.f_un.f_fname, ctty + sizeof _PATH_DEV - 1);
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";
	(void)strcpy(bootfile, getbootfile());
	(void)signal(SIGTERM, die);
	(void)signal(SIGINT, Debug ? die : SIG_IGN);
	(void)signal(SIGQUIT, Debug ? die : SIG_IGN);
	/*
	 * We don't want the SIGCHLD and SIGHUP handlers to interfere
	 * with each other; they are likely candidates for being called
	 * simultaneously (SIGHUP closes pipe descriptor, process dies,
	 * SIGCHLD happens).
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SIGHUP);
	sact.sa_handler = reapchild;
	sact.sa_mask = mask;
	sact.sa_flags = SA_RESTART;
	(void)sigaction(SIGCHLD, &sact, NULL);
	(void)signal(SIGALRM, domark);
	(void)signal(SIGPIPE, SIG_IGN);	/* We'll catch EPIPE instead. */
	(void)alarm(TIMERINTVL);

	TAILQ_INIT(&deadq_head);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	for (i = 0; i < nfunix; i++) {
		memset(&sunx, 0, sizeof(sunx));
		sunx.sun_family = AF_UNIX;
		(void)strncpy(sunx.sun_path, funixn[i], sizeof(sunx.sun_path));
		funix[i] = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (funix[i] < 0 ||
		    bind(funix[i], (struct sockaddr *)&sunx,
			 SUN_LEN(&sunx)) < 0 ||
		    chmod(funixn[i], 0666) < 0) {
			(void) snprintf(line, sizeof line,
					"cannot create %s", funixn[i]);
			logerror(line);
			dprintf("cannot create %s (%d)\n", funixn[i], errno);
			if (i == 0)
				die(0);
		}
	}
	if (SecureMode <= 1)
		finet = socksetup(family);

	if (finet) {
		if (SecureMode) {
			for (i = 0; i < *finet; i++) {
				if (shutdown(finet[i+1], SHUT_RD) < 0) {
					logerror("shutdown");
					if (!Debug)
						die(0);
				}
			}
		} else
			dprintf("listening on inet and/or inet6 socket\n");
		dprintf("sending on inet and/or inet6 socket\n");
	}

	if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
		if (fcntl(fklog, F_SETFL, O_NONBLOCK) < 0)
			fklog = -1;
	if (fklog < 0)
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);

	/* tuck my process id away */
	fp = fopen(PidFile, "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		(void) fclose(fp);
	}

	dprintf("off & running....\n");

	init(0);
	/* prevent SIGHUP and SIGCHLD handlers from running in parallel */
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sact.sa_handler = init;
	sact.sa_mask = mask;
	sact.sa_flags = SA_RESTART;
	(void)sigaction(SIGHUP, &sact, NULL);

	tvp = &tv;
	tv.tv_sec = tv.tv_usec = 0;

	for (;;) {
		fd_set readfds;
		int nfds = 0;

		FD_ZERO(&readfds);
		if (fklog != -1) {
			FD_SET(fklog, &readfds);
			if (fklog > nfds)
				nfds = fklog;
		}
		if (finet && !SecureMode) {
			for (i = 0; i < *finet; i++) {
				FD_SET(finet[i+1], &readfds);
				if (finet[i+1] > nfds)
					nfds = finet[i+1];
			}
		}
		for (i = 0; i < nfunix; i++) {
			if (funix[i] != -1) {
				FD_SET(funix[i], &readfds);
				if (funix[i] > nfds)
					nfds = funix[i];
			}
		}

		/*dprintf("readfds = %#x\n", readfds);*/
		nfds = select(nfds+1, &readfds, (fd_set *)NULL,
			      (fd_set *)NULL, tvp);
		if (nfds == 0) {
			if (tvp) {
				tvp = NULL;
				if (ppid != 1)
					kill(ppid, SIGALRM);
			}
			continue;
		}
		if (nfds < 0) {
			if (errno != EINTR)
				logerror("select");
			continue;
		}
		/*dprintf("got a message (%d, %#x)\n", nfds, readfds);*/
		if (fklog != -1 && FD_ISSET(fklog, &readfds))
			readklog();
		if (finet && !SecureMode) {
			for (i = 0; i < *finet; i++) {
				if (FD_ISSET(finet[i+1], &readfds)) {
					len = sizeof(frominet);
					l = recvfrom(finet[i+1], line, MAXLINE,
					     0, (struct sockaddr *)&frominet,
					     &len);
					if (l > 0) {
						line[l] = '\0';
						hname = cvthname((struct sockaddr *)&frominet);
						unmapped((struct sockaddr *)&frominet);
						if (validate((struct sockaddr *)&frominet, hname))
							printline(hname, line);
					} else if (l < 0 && errno != EINTR)
						logerror("recvfrom inet");
				}
			}
		}
		for (i = 0; i < nfunix; i++) {
			if (funix[i] != -1 && FD_ISSET(funix[i], &readfds)) {
				len = sizeof(fromunix);
				l = recvfrom(funix[i], line, MAXLINE, 0,
				    (struct sockaddr *)&fromunix, &len);
				if (l > 0) {
					line[l] = '\0';
					printline(LocalHostName, line);
				} else if (l < 0 && errno != EINTR)
					logerror("recvfrom unix");
			}
		}
	}
}

static void
unmapped(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in6 *sin6;
	struct sockaddr_in sin;

	if (sa->sa_family != AF_INET6)
		return;
	if (sa->sa_len != sizeof(struct sockaddr_in6) ||
	    sizeof(sin) > sa->sa_len)
		return;
	sin6 = (struct sockaddr_in6 *)sa;
	if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr[12],
	       sizeof(sin.sin_addr));
	sin.sin_port = sin6->sin6_port;

	memcpy(sa, &sin, sin.sin_len);
}

static void
usage()
{

	fprintf(stderr, "%s\n%s\n%s\n",
		"usage: syslogd [-46Adnsuv] [-a allowed_peer] [-f config_file]",
		"               [-m mark_interval] [-l log_socket]",
		"               [-p log_socket] [-P pid_file]");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(hname, msg)
	char *hname;
	char *msg;
{
	int c, pri;
	char *p, *q, line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/* don't allow users to log kernel messages */
	if (LOG_FAC(pri) == LOG_KERN && !KeepKernFac)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	q = line;

	while ((c = (unsigned char)*p++) != '\0' &&
	    q < &line[sizeof(line) - 4]) {
		if ((c & 0x80) && c < 0xA0) {
			c &= 0x7F;
			*q++ = 'M';
			*q++ = '-';
		}
		if (isascii(c) && iscntrl(c)) {
			if (c == '\n')
				*q++ = ' ';
			else if (c == '\t')
				*q++ = '\t';
			else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		} else
			*q++ = c;
	}
	*q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Read /dev/klog while data are available, split into lines.
 */
void
readklog()
{
	char *p, *q, line[MAXLINE + 1];
	int len, i;

	len = 0;
	for (;;) {
		i = read(fklog, line + len, MAXLINE - 1 - len);
		if (i > 0)
			line[i + len] = '\0';
		else if (i < 0 && errno != EINTR && errno != EAGAIN) {
			logerror("klog");
			fklog = -1;
			break;
		} else
			break;

		for (p = line; (q = strchr(p, '\n')) != NULL; p = q + 1) {
			*q = '\0';
			printsys(p);
		}
		len = strlen(p);
		if (len >= MAXLINE - 1) {
			printsys(p);
			len = 0;
		}
		if (len > 0) 
			memmove(line, p, len + 1);
	}
	if (len > 0)
		printsys(line);
}

/*
 * Take a raw input line from /dev/klog, format similar to syslog().
 */
void
printsys(p)
	char *p;
{
	int pri, flags;

	flags = ISKERNEL | SYNC_FILE | ADDDATE;	/* fsync after write */
	pri = DEFSPRI;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
		if ((pri & LOG_FACMASK) == LOG_CONSOLE)
			flags |= IGN_CONS;
	} else {
		/* kernel printf's come out on console */
		flags |= IGN_CONS;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFSPRI;
	logmsg(pri, p, LocalHostName, flags);
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(pri, msg, from, flags)
	int pri;
	char *msg, *from;
	int flags;
{
	struct filed *f;
	int i, fac, msglen, omask, prilev;
	char *timestamp;
 	char prog[NAME_MAX+1];
	char buf[MAXLINE+1];

	dprintf("logmsg: pri %o, flags %x, from %s, msg %s\n",
	    pri, flags, from, msg);

	omask = sigblock(sigmask(SIGHUP)|sigmask(SIGALRM));

	/*
	 * Check to see if msg looks non-standard.
	 */
	msglen = strlen(msg);
	if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	(void)time(&now);
	if (flags & ADDDATE)
		timestamp = ctime(&now) + 4;
	else {
		timestamp = msg;
		msg += 16;
		msglen -= 16;
	}

	/* skip leading blanks */
	while (isspace(*msg)) {
		msg++;
		msglen--;
	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for (i = 0; i < NAME_MAX; i++) {
		if (!isalnum(msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* add kernel prefix for kernel messages */
	if (flags & ISKERNEL) {
		snprintf(buf, sizeof(buf), "%s: %s", bootfile, msg);
		msg = buf;
		msglen = strlen(buf);
	}

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = open(ctty, O_WRONLY, 0);

		if (f->f_file >= 0) {
			fprintlog(f, flags, msg);
			(void)close(f->f_file);
		}
		(void)sigsetmask(omask);
		return;
	}
	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		if (!(((f->f_pcmp[fac] & PRI_EQ) && (f->f_pmask[fac] == prilev))
		     ||((f->f_pcmp[fac] & PRI_LT) && (f->f_pmask[fac] < prilev))
		     ||((f->f_pcmp[fac] & PRI_GT) && (f->f_pmask[fac] > prilev))
		     )
		    || f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;
		/* skip messages with the incorrect hostname */
		if (f->f_host)
			switch (f->f_host[0]) {
			case '+':
				if (strcmp(from, f->f_host + 1) != 0)
					continue;
				break;
			case '-':
				if (strcmp(from, f->f_host + 1) == 0)
					continue;
				break;
			}

		/* skip messages with the incorrect program name */
		if (f->f_program)
			if (strcmp(prog, f->f_program) != 0)
				continue;

		if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if ((flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcasecmp(from, f->f_prevhost)) {
			(void)strncpy(f->f_lasttime, timestamp, 15);
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f, flags, (char *)NULL);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				fprintlog(f, 0, (char *)NULL);
			f->f_repeatcount = 0;
			f->f_prevpri = pri;
			(void)strncpy(f->f_lasttime, timestamp, 15);
			(void)strncpy(f->f_prevhost, from,
					sizeof(f->f_prevhost)-1);
			f->f_prevhost[sizeof(f->f_prevhost)-1] = '\0';
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				(void)strcpy(f->f_prevline, msg);
				fprintlog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, msg);
			}
		}
	}
	(void)sigsetmask(omask);
}

void
fprintlog(f, flags, msg)
	struct filed *f;
	int flags;
	char *msg;
{
	struct iovec iov[7];
	struct iovec *v;
	struct addrinfo *r;
	int i, l, lsent = 0;
	char line[MAXLINE + 1], repbuf[80], greetings[200];
	char *msgret;

	v = iov;
	if (f->f_type == F_WALL) {
		v->iov_base = greetings;
		v->iov_len = snprintf(greetings, sizeof greetings,
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    f->f_prevhost, ctime(&now));
		v++;
		v->iov_base = "";
		v->iov_len = 0;
		v++;
	} else {
		v->iov_base = f->f_lasttime;
		v->iov_len = 15;
		v++;
		v->iov_base = " ";
		v->iov_len = 1;
		v++;
	}

	if (LogFacPri) {
	  	static char fp_buf[30];	/* Hollow laugh */
		int fac = f->f_prevpri & LOG_FACMASK;
		int pri = LOG_PRI(f->f_prevpri);
		const char *f_s = NULL;
		char f_n[5];	/* Hollow laugh */
		const char *p_s = NULL;
		char p_n[5];	/* Hollow laugh */

		if (LogFacPri > 1) {
		  CODE *c;

		  for (c = facilitynames; c->c_name; c++) {
		    if (c->c_val == fac) {
		      f_s = c->c_name;
		      break;
		    }
		  }
		  for (c = prioritynames; c->c_name; c++) {
		    if (c->c_val == pri) {
		      p_s = c->c_name;
		      break;
		    }
		  }
		}
		if (!f_s) {
		  snprintf(f_n, sizeof f_n, "%d", LOG_FAC(fac));
		  f_s = f_n;
		}
		if (!p_s) {
		  snprintf(p_n, sizeof p_n, "%d", pri);
		  p_s = p_n;
		}
		snprintf(fp_buf, sizeof fp_buf, "<%s.%s> ", f_s, p_s);
		v->iov_base = fp_buf;
		v->iov_len = strlen(fp_buf);
	} else {
	        v->iov_base="";
		v->iov_len = 0;
	}
	v++;

	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	if (msg) {
		v->iov_base = msg;
		v->iov_len = strlen(msg);
	} else if (f->f_prevcount > 1) {
		v->iov_base = repbuf;
		v->iov_len = sprintf(repbuf, "last message repeated %d times",
		    f->f_prevcount);
	} else {
		v->iov_base = f->f_prevline;
		v->iov_len = f->f_prevlen;
	}
	v++;

	dprintf("Logging to %s", TypeNames[f->f_type]);
	f->f_time = now;

	switch (f->f_type) {
	case F_UNUSED:
		dprintf("\n");
		break;

	case F_FORW:
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		/* check for local vs remote messages */
		if (strcasecmp(f->f_prevhost, LocalHostName))
			l = snprintf(line, sizeof line - 1,
			    "<%d>%.15s Forwarded from %s: %s",
			    f->f_prevpri, iov[0].iov_base, f->f_prevhost,
			    iov[5].iov_base);
		else
			l = snprintf(line, sizeof line - 1, "<%d>%.15s %s",
			     f->f_prevpri, iov[0].iov_base, iov[5].iov_base);
		if (l > MAXLINE)
			l = MAXLINE;

		if (finet) {
			for (r = f->f_un.f_forw.f_addr; r; r = r->ai_next) {
				for (i = 0; i < *finet; i++) {
#if 0 
					/*
					 * should we check AF first, or just
					 * trial and error? FWD
					 */
					if (r->ai_family ==
					    address_family_of(finet[i+1])) 
#endif
					lsent = sendto(finet[i+1], line, l, 0,
					    r->ai_addr, r->ai_addrlen);
					if (lsent == l) 
						break;
				}
				if (lsent == l && !send_to_all) 
					break;
			}
			if (lsent != l) {
				int e = errno;
				(void)close(f->f_file);
				errno = e;
				f->f_type = F_UNUSED;
				logerror("sendto");
			}
		}
		break;

	case F_FILE:
		dprintf(" %s\n", f->f_un.f_fname);
		v->iov_base = "\n";
		v->iov_len = 1;
		if (writev(f->f_file, iov, 7) < 0) {
			int e = errno;
			(void)close(f->f_file);
			f->f_type = F_UNUSED;
			errno = e;
			logerror(f->f_un.f_fname);
		} else if (flags & SYNC_FILE)
			(void)fsync(f->f_file);
		break;

	case F_PIPE:
		dprintf(" %s\n", f->f_un.f_pipe.f_pname);
		v->iov_base = "\n";
		v->iov_len = 1;
		if (f->f_un.f_pipe.f_pid == 0) {
			if ((f->f_file = p_open(f->f_un.f_pipe.f_pname,
						&f->f_un.f_pipe.f_pid)) < 0) {
				f->f_type = F_UNUSED;
				logerror(f->f_un.f_pipe.f_pname);
				break;
			}
		}
		if (writev(f->f_file, iov, 7) < 0) {
			int e = errno;
			(void)close(f->f_file);
			if (f->f_un.f_pipe.f_pid > 0)
				deadq_enter(f->f_un.f_pipe.f_pid,
					    f->f_un.f_pipe.f_pname);
			f->f_un.f_pipe.f_pid = 0;
			errno = e;
			logerror(f->f_un.f_pipe.f_pname);
		}
		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(" (ignored)\n");
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
		dprintf(" %s%s\n", _PATH_DEV, f->f_un.f_fname);
		v->iov_base = "\r\n";
		v->iov_len = 2;

		errno = 0;	/* ttymsg() only sometimes returns an errno */
		if ((msgret = ttymsg(iov, 7, f->f_un.f_fname, 10))) {
			f->f_type = F_UNUSED;
			logerror(msgret);
		}
		break;

	case F_USERS:
	case F_WALL:
		dprintf("\n");
		v->iov_base = "\r\n";
		v->iov_len = 2;
		wallmsg(f, iov);
		break;
	}
	f->f_prevcount = 0;
}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(f, iov)
	struct filed *f;
	struct iovec *iov;
{
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	struct utmp ut;
	int i;
	char *p;
	char line[sizeof(ut.ut_line) + 1];

	if (reenter++)
		return;
	if ((uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread((char *)&ut, sizeof(ut), 1, uf) == 1) {
		if (ut.ut_name[0] == '\0')
			continue;
		strncpy(line, ut.ut_line, sizeof(ut.ut_line));
		line[sizeof(ut.ut_line)] = '\0';
		if (f->f_type == F_WALL) {
			if ((p = ttymsg(iov, 7, line, TTYMSGTIME)) != NULL) {
				errno = 0;	/* already in msg */
				logerror(p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!f->f_un.f_uname[i][0])
				break;
			if (!strncmp(f->f_un.f_uname[i], ut.ut_name,
			    UT_NAMESIZE)) {
				if ((p = ttymsg(iov, 7, line, TTYMSGTIME))
								!= NULL) {
					errno = 0;	/* already in msg */
					logerror(p);
				}
				break;
			}
		}
	}
	(void)fclose(uf);
	reenter = 0;
}

void
reapchild(signo)
	int signo;
{
	int status;
	pid_t pid;
	struct filed *f;

	while ((pid = wait3(&status, WNOHANG, (struct rusage *)NULL)) > 0) {
		if (!Initialized)
			/* Don't tell while we are initting. */
			continue;

		/* First, look if it's a process from the dead queue. */
		if (deadq_remove(pid))
			goto oncemore;

		/* Now, look in list of active processes. */
		for (f = Files; f; f = f->f_next)
			if (f->f_type == F_PIPE &&
			    f->f_un.f_pipe.f_pid == pid) {
				(void)close(f->f_file);
				f->f_un.f_pipe.f_pid = 0;
				log_deadchild(pid, status,
					      f->f_un.f_pipe.f_pname);
				break;
			}
	  oncemore:
		continue;
	}
}

/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr *f;
{
	int error;
	sigset_t omask, nmask;
	char *p;
	static char hname[NI_MAXHOST], ip[NI_MAXHOST];

	error = getnameinfo((struct sockaddr *)f,
			    ((struct sockaddr *)f)->sa_len,
			    ip, sizeof ip, NULL, 0,
			    NI_NUMERICHOST | withscopeid);
	dprintf("cvthname(%s)\n", ip);

	if (error) {
		dprintf("Malformed from address %s\n", gai_strerror(error));
		return ("???");
	}
	if (!resolve)
		return (ip);

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	error = getnameinfo((struct sockaddr *)f,
			    ((struct sockaddr *)f)->sa_len,
			    hname, sizeof hname, NULL, 0,
			    NI_NAMEREQD | withscopeid);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (error) {
		dprintf("Host name for your address (%s) unknown\n", ip);
		return (ip);
	}
	if ((p = strchr(hname, '.')) && strcasecmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hname);
}

void
domark(signo)
	int signo;
{
	struct filed *f;
	dq_t q;

	now = time((time_t *)NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}

	/* Walk the dead queue, and see if we should signal somebody. */
	for (q = TAILQ_FIRST(&deadq_head); q != NULL; q = TAILQ_NEXT(q, dq_entries))
		switch (q->dq_timeout) {
		case 0:
			/* Already signalled once, try harder now. */
			if (kill(q->dq_pid, SIGKILL) != 0)
				(void)deadq_remove(q->dq_pid);
			break;

		case 1:
			/*
			 * Timed out on dead queue, send terminate
			 * signal.  Note that we leave the removal
			 * from the dead queue to reapchild(), which
			 * will also log the event (unless the process
			 * didn't even really exist, in case we simply
			 * drop it from the dead queue).
			 */
			if (kill(q->dq_pid, SIGTERM) != 0)
				(void)deadq_remove(q->dq_pid);
			/* FALLTHROUGH */

		default:
			q->dq_timeout--;
		}

	(void)alarm(TIMERINTVL);
}

/*
 * Print syslogd errors some place.
 */
void
logerror(type)
	const char *type;
{
	char buf[512];

	if (errno)
		(void)snprintf(buf,
		    sizeof buf, "syslogd: %s: %s", type, strerror(errno));
	else
		(void)snprintf(buf, sizeof buf, "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", buf);
	logmsg(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
die(signo)
	int signo;
{
	struct filed *f;
	int was_initialized;
	char buf[100];
	int i;

	was_initialized = Initialized;
	Initialized = 0;	/* Don't log SIGCHLDs. */
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);
		if (f->f_type == F_PIPE)
			(void)close(f->f_file);
	}
	Initialized = was_initialized;
	if (signo) {
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)sprintf(buf, "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}
	for (i = 0; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			(void)unlink(funixn[i]);
	exit(1);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(signo)
	int signo;
{
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];
	char host[MAXHOSTNAMELEN];

	dprintf("init\n");

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FILE:
		case F_FORW:
		case F_CONSOLE:
		case F_TTY:
			(void)close(f->f_file);
			break;
		case F_PIPE:
			(void)close(f->f_file);
			if (f->f_un.f_pipe.f_pid > 0)
				deadq_enter(f->f_un.f_pipe.f_pid,
					    f->f_un.f_pipe.f_pname);
			f->f_un.f_pipe.f_pid = 0;
			break;
		}
		next = f->f_next;
		if (f->f_program) free(f->f_program);
		if (f->f_host) free(f->f_host);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.ERR\t/dev/console", *nextp, "*", "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.PANIC\t*", (*nextp)->f_next, "*", "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strcpy(host, "*");
	strcpy(prog, "*");
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog is treated specially:
		 * following lines apply only to that program.
		 */
		for (p = cline; isspace(*p); ++p)
			continue;
		if (*p == 0)
			continue;
		if (*p == '#') {
			p++;
			if (*p != '!' && *p != '+' && *p != '-')
				continue;
		}
		if (*p == '+' || *p == '-') {
			host[0] = *p++;
			while (isspace(*p)) p++;
			if ((!*p) || (*p == '*')) {
				strcpy(host, "*");
				continue;
			}
			if (*p == '@')
				p = LocalHostName;
			for (i = 1; i < MAXHOSTNAMELEN - 1; i++) {
				if (!isalnum(*p) && *p != '.' && *p != '-')
					break;
				host[i] = *p++;
			}
			host[i] = '\0';
			continue;
		}
		if (*p == '!') {
			p++;
			while (isspace(*p)) p++;
			if ((!*p) || (*p == '*')) {
				strcpy(prog, "*");
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum(p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		for (p = strchr(cline, '\0'); isspace(*--p);)
			continue;
		*++p = '\0';
		f = (struct filed *)calloc(1, sizeof(*f));
		*nextp = f;
		nextp = &f->f_next;
		cfline(cline, f, prog, host);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_CONSOLE:
			case F_TTY:
				printf("%s%s", _PATH_DEV, f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;

			case F_PIPE:
				printf("%s", f->f_un.f_pipe.f_pname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;
			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	dprintf("syslogd: restarted\n");
}

/*
 * Crack a configuration file line
 */
void
cfline(line, f, prog, host)
	char *line;
	struct filed *f;
	char *prog;
	char *host;
{
	struct addrinfo hints, *res;
	int error, i, pri;
	char *bp, *p, *q;
	char buf[MAXLINE], ebuf[100];

	dprintf("cfline(\"%s\", f, \"%s\", \"%s\")\n", line, prog, host);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save hostname if any */
	if (host && *host == '*')
		host = NULL;
	if (host)
		f->f_host = strdup(host);

	/* save program name if any */
	if (prog && *prog == '*')
		prog = NULL;
	if (prog)
		f->f_program = strdup(prog);

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t' && *p != ' ';) {
		int pri_done;
		int pri_cmp;

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q != ' ' && *q++ != '.'; )
			continue;

		/* get the priority comparison */
		pri_cmp = 0;
		pri_done = 0;
		while (!pri_done) {
			switch (*q) {
			case '<':
				pri_cmp |= PRI_LT;
				q++;
				break;
			case '=':
				pri_cmp |= PRI_EQ;
				q++;
				break;
			case '>':
				pri_cmp |= PRI_GT;
				q++;
				break;
			default:
				pri_done++;
				break;
			}
		}
		if (!pri_cmp)
			pri_cmp = (UniquePriority)
				  ? (PRI_EQ)
				  : (PRI_EQ | PRI_GT)
				  ;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,; ", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(",;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*')
			pri = LOG_PRIMASK + 1;
		else {
			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				return;
			}
		}

		/* scan facilities */
		while (*p && !strchr("\t.; ", *p)) {
			for (bp = buf; *p && !strchr("\t,;. ", *p); )
				*bp++ = *p++;
			*bp = '\0';

			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++) {
					f->f_pmask[i] = pri;
					f->f_pcmp[i] = pri_cmp;
				}
			else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					(void)snprintf(ebuf, sizeof ebuf,
					    "unknown facility name \"%s\"",
					    buf);
					logerror(ebuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
				f->f_pcmp[i >> 3] = pri_cmp;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	switch (*p)
	{
	case '@':
		(void)strncpy(f->f_un.f_forw.f_hname, ++p,
			sizeof(f->f_un.f_forw.f_hname)-1);
		f->f_un.f_forw.f_hname[sizeof(f->f_un.f_forw.f_hname)-1] = '\0';
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM;
		error = getaddrinfo(f->f_un.f_forw.f_hname, "syslog", &hints,
				    &res);
		if (error) {
			logerror(gai_strerror(error));
			break;
		}
		f->f_un.f_forw.f_addr = res;
		f->f_type = F_FORW;
		break;

	case '/':
		if ((f->f_file = open(p, O_WRONLY|O_APPEND, 0)) < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			if (strcmp(p, ctty) == 0)
				f->f_type = F_CONSOLE;
			else
				f->f_type = F_TTY;
			(void)strcpy(f->f_un.f_fname, p + sizeof _PATH_DEV - 1);
		} else {
			(void)strcpy(f->f_un.f_fname, p);
			f->f_type = F_FILE;
		}
		break;

	case '|':
		f->f_un.f_pipe.f_pid = 0;
		(void)strcpy(f->f_un.f_pipe.f_pname, p + 1);
		f->f_type = F_PIPE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(f->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				f->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(name, codetab)
	const char *name;
	CODE *codetab;
{
	CODE *c;
	char *p, buf[40];

	if (isdigit(*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper(*name))
			*p = tolower(*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

/*
 * fork off and become a daemon, but wait for the child to come online
 * before returing to the parent, or we get disk thrashing at boot etc.
 * Set a timer so we don't hang forever if it wedges.
 */
int
waitdaemon(nochdir, noclose, maxwait)
	int nochdir, noclose, maxwait;
{
	int fd;
	int status;
	pid_t pid, childpid;

	switch (childpid = fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		signal(SIGALRM, timedout);
		alarm(maxwait);
		while ((pid = wait3(&status, 0, NULL)) != -1) {
			if (WIFEXITED(status))
				errx(1, "child pid %d exited with return code %d",
					pid, WEXITSTATUS(status));
			if (WIFSIGNALED(status))
				errx(1, "child pid %d exited on signal %d%s",
					pid, WTERMSIG(status),
					WCOREDUMP(status) ? " (core dumped)" :
					"");
			if (pid == childpid)	/* it's gone... */
				break;
		}
		exit(0);
	}

	if (setsid() == -1)
		return (-1);

	if (!nochdir)
		(void)chdir("/");

	if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close (fd);
	}
	return (getppid());
}

/*
 * We get a SIGALRM from the child when it's running and finished doing it's
 * fsync()'s or O_SYNC writes for all the boot messages.
 *
 * We also get a signal from the kernel if the timer expires, so check to
 * see what happened.
 */
void
timedout(sig)
	int sig __unused;
{
	int left;
	left = alarm(0);
	signal(SIGALRM, SIG_DFL);
	if (left == 0)
		errx(1, "timed out waiting for child");
	else
		exit(0);
}

/*
 * Add `s' to the list of allowable peer addresses to accept messages
 * from.
 *
 * `s' is a string in the form:
 *
 *    [*]domainname[:{servicename|portnumber|*}]
 *
 * or
 *
 *    netaddr/maskbits[:{servicename|portnumber|*}]
 *
 * Returns -1 on error, 0 if the argument was valid.
 */
int
allowaddr(s)
	char *s;
{
	char *cp1, *cp2;
	struct allowedpeer ap;
	struct servent *se;
	int masklen = -1;
	struct addrinfo hints, *res;
	struct in_addr *addrp, *maskp;
	u_int32_t *mask6p;
	char ip[NI_MAXHOST];

#ifdef INET6
	if (*s != '[' || (cp1 = strchr(s + 1, ']')) == NULL)
#endif
		cp1 = s;
	if ((cp1 = strrchr(cp1, ':'))) {
		/* service/port provided */
		*cp1++ = '\0';
		if (strlen(cp1) == 1 && *cp1 == '*')
			/* any port allowed */
			ap.port = 0;
		else if ((se = getservbyname(cp1, "udp")))
			ap.port = ntohs(se->s_port);
		else {
			ap.port = strtol(cp1, &cp2, 0);
			if (*cp2 != '\0')
				return -1; /* port not numeric */
		}
	} else {
		if ((se = getservbyname("syslog", "udp")))
			ap.port = ntohs(se->s_port);
		else
			/* sanity, should not happen */
			ap.port = 514;
	}

	if ((cp1 = strchr(s, '/')) != NULL &&
	    strspn(cp1 + 1, "0123456789") == strlen(cp1 + 1)) {
		*cp1 = '\0';
		if ((masklen = atoi(cp1 + 1)) < 0)
			return -1;
	}
#ifdef INET6
	if (*s == '[') {
		cp2 = s + strlen(s) - 1;
		if (*cp2 == ']') {
			++s;
			*cp2 = '\0';
		} else
			cp2 = NULL;
	} else
		cp2 = NULL;
#endif
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if (getaddrinfo(s, NULL, &hints, &res) == 0) {
		ap.isnumeric = 1;
		memcpy(&ap.a_addr, res->ai_addr, res->ai_addrlen);
		memset(&ap.a_mask, 0, sizeof(ap.a_mask));
		ap.a_mask.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			ap.a_mask.ss_len = sizeof(struct sockaddr_in);
			maskp = &((struct sockaddr_in *)&ap.a_mask)->sin_addr;
			if (masklen < 0) {
				/* use default netmask */
				addrp = &((struct sockaddr_in *)&ap.a_addr)->sin_addr;
				if (IN_CLASSA(ntohl(addrp->s_addr)))
					maskp->s_addr = htonl(IN_CLASSA_NET);
				else if (IN_CLASSB(ntohl(addrp->s_addr)))
					maskp->s_addr = htonl(IN_CLASSB_NET);
				else
					maskp->s_addr = htonl(IN_CLASSC_NET);
			} else if (masklen <= 32) {
				/* convert masklen to netmask */
				maskp->s_addr = htonl(~((1 << (32 - masklen)) - 1));
			} else {
				freeaddrinfo(res);
				return -1;
			}
		}
#ifdef INET6
		else if (res->ai_family == AF_INET6 && masklen <= 128) {
			ap.a_mask.ss_len = sizeof(struct sockaddr_in6);
			if (masklen < 0)
				masklen = 128;
			mask6p = (u_int32_t *)&((struct sockaddr_in6 *)&ap.a_mask)->sin6_addr;
			/* convert masklen to netmask */
			while (masklen > 0) {
				if (masklen < 32) {
					*mask6p = htonl(~(0xffffffff >> masklen));
					break;
				}
				*mask6p++ = 0xffffffff;
				masklen -= 32;
			}
		}
#endif
		else {
			freeaddrinfo(res);
			return -1;
		}
		freeaddrinfo(res);
	} else {
		/* arg `s' is domain name */
		ap.isnumeric = 0;
		ap.a_name = s;
		if (cp1)
			*cp1 = '/';
#ifdef INET6
		if (cp2) {
			*cp2 = ']';
			--s;
		}
#endif
	}

	if (Debug) {
		printf("allowaddr: rule %d: ", NumAllowed);
		if (ap.isnumeric) {
			printf("numeric, ");
			getnameinfo((struct sockaddr *)&ap.a_addr,
				    ((struct sockaddr *)&ap.a_addr)->sa_len,
				    ip, sizeof ip, NULL, 0,
				    NI_NUMERICHOST | withscopeid);
			printf("addr = %s, ", ip);
			getnameinfo((struct sockaddr *)&ap.a_mask,
				    ((struct sockaddr *)&ap.a_mask)->sa_len,
				    ip, sizeof ip, NULL, 0,
				    NI_NUMERICHOST | withscopeid);
			printf("mask = %s; ", ip);
		} else
			printf("domainname = %s; ", ap.a_name);
		printf("port = %d\n", ap.port);
	}

	if ((AllowedPeers = realloc(AllowedPeers,
				    ++NumAllowed * sizeof(struct allowedpeer)))
	    == NULL) {
		fprintf(stderr, "Out of memory!\n");
		exit(EX_OSERR);
	}
	memcpy(&AllowedPeers[NumAllowed - 1], &ap, sizeof(struct allowedpeer));
	return 0;
}

/*
 * Validate that the remote peer has permission to log to us.
 */
int
validate(sa, hname)
	struct sockaddr *sa;
	const char *hname;
{
	int i, j, reject;
	size_t l1, l2;
	char *cp, name[NI_MAXHOST], ip[NI_MAXHOST], port[NI_MAXSERV];
	struct allowedpeer *ap;
	struct sockaddr_in *sin, *a4p = NULL, *m4p = NULL;
	struct sockaddr_in6 *sin6, *a6p = NULL, *m6p = NULL;
	struct addrinfo hints, *res;
	u_short sport;

	if (NumAllowed == 0)
		/* traditional behaviour, allow everything */
		return 1;

	strlcpy(name, hname, sizeof name);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if (getaddrinfo(name, NULL, &hints, &res) == 0)
		freeaddrinfo(res);
	else if (strchr(name, '.') == NULL) {
		strlcat(name, ".", sizeof name);
		strlcat(name, LocalDomain, sizeof name);
	}
	if (getnameinfo(sa, sa->sa_len, ip, sizeof ip, port, sizeof port,
			NI_NUMERICHOST | withscopeid | NI_NUMERICSERV) != 0)
		return 0;	/* for safety, should not occur */
	dprintf("validate: dgram from IP %s, port %s, name %s;\n",
		ip, port, name);
	sport = atoi(port);

	/* now, walk down the list */
	for (i = 0, ap = AllowedPeers; i < NumAllowed; i++, ap++) {
		if (ap->port != 0 && ap->port != sport) {
			dprintf("rejected in rule %d due to port mismatch.\n", i);
			continue;
		}

		if (ap->isnumeric) {
			if (ap->a_addr.ss_family != sa->sa_family) {
				dprintf("rejected in rule %d due to address family mismatch.\n", i);
				continue;
			}
			if (ap->a_addr.ss_family == AF_INET) {
				sin = (struct sockaddr_in *)sa;
				a4p = (struct sockaddr_in *)&ap->a_addr;
				m4p = (struct sockaddr_in *)&ap->a_mask;
				if ((sin->sin_addr.s_addr & m4p->sin_addr.s_addr)
				    != a4p->sin_addr.s_addr) {
					dprintf("rejected in rule %d due to IP mismatch.\n", i);
					continue;
				}
			}
#ifdef INET6
			else if (ap->a_addr.ss_family == AF_INET6) {
				sin6 = (struct sockaddr_in6 *)sa;
				a6p = (struct sockaddr_in6 *)&ap->a_addr;
				m6p = (struct sockaddr_in6 *)&ap->a_mask;
#ifdef NI_WITHSCOPEID
				if (a6p->sin6_scope_id != 0 &&
				    sin6->sin6_scope_id != a6p->sin6_scope_id) {
					dprintf("rejected in rule %d due to scope mismatch.\n", i);
					continue;
				}
#endif
				reject = 0;
				for (j = 0; j < 16; j += 4) {
					if ((*(u_int32_t *)&sin6->sin6_addr.s6_addr[i] & *(u_int32_t *)&m6p->sin6_addr.s6_addr[i])
					    != *(u_int32_t *)&a6p->sin6_addr.s6_addr[i]) {
						++reject;
						break;
					}
				}
				if (reject) {
					dprintf("rejected in rule %d due to IP mismatch.\n", i);
					continue;
				}
			}
#endif
			else
				continue;
		} else {
			cp = ap->a_name;
			l1 = strlen(name);
			if (*cp == '*') {
				/* allow wildmatch */
				cp++;
				l2 = strlen(cp);
				if (l2 > l1 || memcmp(cp, &name[l1 - l2], l2) != 0) {
					dprintf("rejected in rule %d due to name mismatch.\n", i);
					continue;
				}
			} else {
				/* exact match */
				l2 = strlen(cp);
				if (l2 != l1 || memcmp(cp, name, l1) != 0) {
					dprintf("rejected in rule %d due to name mismatch.\n", i);
					continue;
				}
			}
		}
		dprintf("accepted in rule %d.\n", i);
		return 1;	/* hooray! */
	}
	return 0;
}

/*
 * Fairly similar to popen(3), but returns an open descriptor, as
 * opposed to a FILE *.
 */
int
p_open(prog, pid)
	char *prog;
	pid_t *pid;
{
	int pfd[2], nulldesc, i;
	sigset_t omask, mask;
	char *argv[4]; /* sh -c cmd NULL */
	char errmsg[200];

	if (pipe(pfd) == -1)
		return -1;
	if ((nulldesc = open(_PATH_DEVNULL, O_RDWR)) == -1)
		/* we are royally screwed anyway */
		return -1;

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	switch ((*pid = fork())) {
	case -1:
		sigprocmask(SIG_SETMASK, &omask, 0);
		close(nulldesc);
		return -1;

	case 0:
		argv[0] = "sh";
		argv[1] = "-c";
		argv[2] = prog;
		argv[3] = NULL;

		alarm(0);
		(void)setsid();	/* Avoid catching SIGHUPs. */

		/*
		 * Throw away pending signals, and reset signal
		 * behaviour to standard values.
		 */
		signal(SIGALRM, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		sigprocmask(SIG_SETMASK, &omask, 0);
		signal(SIGPIPE, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGALRM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);

		dup2(pfd[0], STDIN_FILENO);
		dup2(nulldesc, STDOUT_FILENO);
		dup2(nulldesc, STDERR_FILENO);
		for (i = getdtablesize(); i > 2; i--)
			(void) close(i);

		(void) execvp(_PATH_BSHELL, argv);
		_exit(255);
	}

	sigprocmask(SIG_SETMASK, &omask, 0);
	close(nulldesc);
	close(pfd[0]);
	/*
	 * Avoid blocking on a hung pipe.  With O_NONBLOCK, we are
	 * supposed to get an EWOULDBLOCK on writev(2), which is
	 * caught by the logic above anyway, which will in turn close
	 * the pipe, and fork a new logging subprocess if necessary.
	 * The stale subprocess will be killed some time later unless
	 * it terminated itself due to closing its input pipe (so we
	 * get rid of really dead puppies).
	 */
	if (fcntl(pfd[1], F_SETFL, O_NONBLOCK) == -1) {
		/* This is bad. */
		(void)snprintf(errmsg, sizeof errmsg,
			       "Warning: cannot change pipe to PID %d to "
			       "non-blocking behaviour.",
			       (int)*pid);
		logerror(errmsg);
	}
	return pfd[1];
}

void
deadq_enter(pid, name)
	pid_t pid;
	const char *name;
{
	dq_t p;
	int status;

	/*
	 * Be paranoid, if we can't signal the process, don't enter it
	 * into the dead queue (perhaps it's already dead).  If possible,
	 * we try to fetch and log the child's status.
	 */
	if (kill(pid, 0) != 0) {
		if (waitpid(pid, &status, WNOHANG) > 0)
			log_deadchild(pid, status, name);
		return;
	}

	p = malloc(sizeof(struct deadq_entry));
	if (p == 0) {
		errno = 0;
		logerror("panic: out of virtual memory!");
		exit(1);
	}

	p->dq_pid = pid;
	p->dq_timeout = DQ_TIMO_INIT;
	TAILQ_INSERT_TAIL(&deadq_head, p, dq_entries);
}

int
deadq_remove(pid)
	pid_t pid;
{
	dq_t q;

	for (q = TAILQ_FIRST(&deadq_head); q != NULL; q = TAILQ_NEXT(q, dq_entries))
		if (q->dq_pid == pid) {
			TAILQ_REMOVE(&deadq_head, q, dq_entries);
				free(q);
				return 1;
		}

	return 0;
}

void
log_deadchild(pid, status, name)
	pid_t pid;
	int status;
	const char *name;
{
	int code;
	char buf[256];
	const char *reason;

	errno = 0; /* Keep strerror() stuff out of logerror messages. */
	if (WIFSIGNALED(status)) {
		reason = "due to signal";
		code = WTERMSIG(status);
	} else {
		reason = "with status";
		code = WEXITSTATUS(status);
		if (code == 0)
			return;
	}
	(void)snprintf(buf, sizeof buf,
		       "Logging subprocess %d (%s) exited %s %d.",
		       pid, name, reason, code);
	logerror(buf);
}

int *
socksetup(af)
	int af;
{
	struct addrinfo hints, *res, *r;
	int error, maxs, *s, *socks;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = af;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(NULL, "syslog", &hints, &res);
	if (error) {
		logerror(gai_strerror(error));
		errno = 0;
		die(0);
	}

	/* Count max number of sockets we may open */
	for (maxs = 0, r = res; r; r = r->ai_next, maxs++);
	socks = malloc((maxs+1) * sizeof(int));
	if (!socks) {
		logerror("couldn't allocate memory for sockets");
		die(0);
	}

	*socks = 0;   /* num of sockets counter at start of array */
	s = socks + 1;
	for (r = res; r; r = r->ai_next) {
		*s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (*s < 0) {
			logerror("socket");
			continue;
		}
#ifdef IPV6_BINDV6ONLY
		if (r->ai_family == AF_INET6) {
			int on = 1;
			if (setsockopt(*s, IPPROTO_IPV6, IPV6_BINDV6ONLY,
				       (char *)&on, sizeof (on)) < 0) {
				logerror("setsockopt");
				close(*s);
				continue;
			}
		}
#endif
		if (bind(*s, r->ai_addr, r->ai_addrlen) < 0) {
			close(*s);
			logerror("bind");
			continue;
		}

		(*socks)++;
		s++;
	}

	if (*socks == 0) {
		free(socks);
		if (Debug)
			return(NULL);
		else
			die(0);
	}
	if (res)
		freeaddrinfo(res);

	return(socks);
}
