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
	"$Id$";
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
#include <sys/msgbuf.h>
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
#include <regex.h>
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

const char	*LogName = _PATH_LOG;
const char	*ConfFile = _PATH_LOGCONF;
const char	*PidFile = _PATH_LOGPID;
const char	ctty[] = _PATH_CONSOLE;

#define FDMASK(fd)	(1 << (fd))

#define	dprintf		if (Debug) printf

#define MAXUNAMES	20	/* maximum number of user names */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN+1];
			struct sockaddr_in	f_addr;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
		struct {
			char	f_pname[MAXPATHLEN];
			pid_t	f_pid;
		} f_pipe;
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN+1];	/* host from which recd. */
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
			struct in_addr addr;
			struct in_addr mask;
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
char	LocalHostName[MAXHOSTNAMELEN+1];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	finet;			/* Internet datagram socket */
int	LogPort;		/* port number for INET connections */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 0;		/* when true, speak only unix domain socks */

int     created_lsock = 0;      /* Flag if local socket created */
char	bootfile[MAXLINE+1];	/* booted kernel file */

struct allowedpeer *AllowedPeers;
int	NumAllowed = 0;		/* # of AllowedPeer entries */

int	allowaddr __P((char *));
void	cfline __P((char *, struct filed *, char *));
char   *cvthname __P((struct sockaddr_in *));
void	deadq_enter __P((pid_t));
int	decode __P((const char *, CODE *));
void	die __P((int));
void	domark __P((int));
void	fprintlog __P((struct filed *, int, char *));
void	init __P((int));
void	logerror __P((const char *));
void	logmsg __P((int, char *, char *, int));
void	printline __P((char *, char *));
void	printsys __P((char *));
int	p_open __P((char *, pid_t *));
void	reapchild __P((int));
char   *ttymsg __P((struct iovec *, int, char *, int));
static void	usage __P((void));
int	validate __P((struct sockaddr_in *, const char *));
void	wallmsg __P((struct filed *, struct iovec *));
int	waitdaemon __P((int, int, int));
void	timedout __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, funix, i, inetm, fklog, klogm, len;
	struct sockaddr_un sunx, fromunix;
	struct sockaddr_in sin, frominet;
	FILE *fp;
	char *p, *hname, line[MSG_BSIZE + 1];
	struct timeval tv, *tvp;
	pid_t ppid;

	while ((ch = getopt(argc, argv, "a:dsf:m:p:")) != -1)
		switch(ch) {
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
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'p':		/* path */
			LogName = optarg;
			break;
		case 's':		/* no network mode */
			SecureMode++;
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
	(void)signal(SIGCHLD, reapchild);
	(void)signal(SIGALRM, domark);
	(void)signal(SIGPIPE, SIG_IGN);	/* We'll catch EPIPE instead. */
	(void)alarm(TIMERINTVL);

	TAILQ_INIT(&deadq_head);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void)strncpy(sunx.sun_path, LogName, sizeof(sunx.sun_path));
	(void)unlink(LogName);
	funix = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (funix < 0 ||
	    bind(funix, (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(LogName, 0666) < 0) {
		(void) snprintf(line, sizeof line, "cannot create %s", LogName);
		logerror(line);
		dprintf("cannot create %s (%d)\n", LogName, errno);
		die(0);
	} else
		created_lsock = 1;

	inetm = 0;
	finet = socket(AF_INET, SOCK_DGRAM, 0);
	if (finet >= 0) {
		struct servent *sp;

		sp = getservbyname("syslog", "udp");
		if (sp == NULL) {
			errno = 0;
			logerror("syslog/udp: unknown service");
			die(0);
		}
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = LogPort = sp->s_port;

		if (!SecureMode) {
		    if (bind(finet, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			    logerror("bind");
			    if (!Debug)
				    die(0);
		    } else {
			    inetm = FDMASK(finet);
		    }
		}
	}
	if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
		klogm = FDMASK(fklog);
	else {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
		klogm = 0;
	}

	/* tuck my process id away */
	fp = fopen(PidFile, "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		(void) fclose(fp);
	}

	dprintf("off & running....\n");

	init(0);
	(void)signal(SIGHUP, init);

	tvp = &tv;
	tv.tv_sec = tv.tv_usec = 0;

	for (;;) {
		int nfds, readfds = FDMASK(funix) | inetm | klogm;

		dprintf("readfds = %#x\n", readfds);
		nfds = select(20, (fd_set *)&readfds, (fd_set *)NULL,
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
		dprintf("got a message (%d, %#x)\n", nfds, readfds);
		if (readfds & klogm) {
			i = read(fklog, line, sizeof(line) - 1);
			if (i > 0) {
				line[i] = '\0';
				printsys(line);
			} else if (i < 0 && errno != EINTR) {
				logerror("klog");
				fklog = -1;
				klogm = 0;
			}
		}
		if (readfds & FDMASK(funix)) {
			len = sizeof(fromunix);
			i = recvfrom(funix, line, MAXLINE, 0,
			    (struct sockaddr *)&fromunix, &len);
			if (i > 0) {
				line[i] = '\0';
				printline(LocalHostName, line);
			} else if (i < 0 && errno != EINTR)
				logerror("recvfrom unix");
		}
		if (readfds & inetm) {
			len = sizeof(frominet);
			i = recvfrom(finet, line, MAXLINE, 0,
			    (struct sockaddr *)&frominet, &len);
			if (i > 0) {
				line[i] = '\0';
				hname = cvthname(&frominet);
				if (validate(&frominet, hname))
					printline(hname, line);
			} else if (i < 0 && errno != EINTR)
				logerror("recvfrom inet");
		}
	}
}

static void
usage()
{

	fprintf(stderr, "%s\n%s\n",
		"usage: syslogd [-ds] [-a allowed_peer] [-f config_file]",
		"               [-m mark_interval] [-p log_socket]");
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
	if (LOG_FAC(pri) == LOG_KERN)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	q = line;

	while ((c = *p++ & 0177) != '\0' &&
	    q < &line[sizeof(line) - 1])
		if (iscntrl(c))
			if (c == '\n')
				*q++ = ' ';
			else if (c == '\t')
				*q++ = '\t';
			else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		else
			*q++ = c;
	*q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(msg)
	char *msg;
{
	int c, pri, flags;
	char *lp, *p, *q, line[MAXLINE + 1];

	(void)strcpy(line, bootfile);
	(void)strcat(line, ": ");
	lp = line + strlen(line);
	for (p = msg; *p != '\0'; ) {
		flags = SYNC_FILE | ADDDATE;	/* fsync file after write */
		pri = DEFSPRI;
		if (*p == '<') {
			pri = 0;
			while (isdigit(*++p))
				pri = 10 * pri + (*p - '0');
			if (*p == '>')
				++p;
		} else {
			/* kernel printf's come out on console */
			flags |= IGN_CONS;
		}
		if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
			pri = DEFSPRI;
		q = lp;
		while (*p != '\0' && (c = *p++) != '\n' &&
		    q < &line[MAXLINE])
			*q++ = c;
		*q = '\0';
		logmsg(pri, line, LocalHostName, flags);
	}
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
	int fac, msglen, omask, prilev;
	char *timestamp;
 	char prog[NAME_MAX+1];
 	int i;

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
	while(isspace(*msg)) {
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
	for(i = 0; i < NAME_MAX; i++) {
		if(!isalnum(msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

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
		if (f->f_pmask[fac] < prilev ||
		    f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;
		/* skip messages with the incorrect program name */
		if(f->f_program)
			if(strcmp(prog, f->f_program) != 0)
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
		    !strcmp(from, f->f_prevhost)) {
			(void)strncpy(f->f_lasttime, timestamp, 15);
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, now - f->f_time,
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
					sizeof(f->f_prevhost));
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
	struct iovec iov[6];
	struct iovec *v;
	int l;
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
		l = snprintf(line, sizeof line - 1, "<%d>%.15s %s",
			     f->f_prevpri, iov[0].iov_base, iov[4].iov_base);
		if (l > MAXLINE)
			l = MAXLINE;
		if ((finet >= 0) &&
		     (sendto(finet, line, l, 0,
			     (struct sockaddr *)&f->f_un.f_forw.f_addr,
			     sizeof(f->f_un.f_forw.f_addr)) != l)) {
			int e = errno;
			(void)close(f->f_file);
			f->f_type = F_UNUSED;
			errno = e;
			logerror("sendto");
		}
		break;

	case F_FILE:
		dprintf(" %s\n", f->f_un.f_fname);
		v->iov_base = "\n";
		v->iov_len = 1;
		if (writev(f->f_file, iov, 6) < 0) {
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
		if (writev(f->f_file, iov, 6) < 0) {
			int e = errno;
			(void)close(f->f_file);
			if (f->f_un.f_pipe.f_pid > 0)
				deadq_enter(f->f_un.f_pipe.f_pid);
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
		if ((msgret = ttymsg(iov, 6, f->f_un.f_fname, 10))) {
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
			if ((p = ttymsg(iov, 6, line, TTYMSGTIME)) != NULL) {
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
				if ((p = ttymsg(iov, 6, line, TTYMSGTIME))
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
	int status, code;
	pid_t pid;
	struct filed *f;
	char buf[256];
	const char *reason;
	dq_t q;

	while ((pid = wait3(&status, WNOHANG, (struct rusage *)NULL)) > 0) {
		if (!Initialized)
			/* Don't tell while we are initting. */
			continue;

		/* First, look if it's a process from the dead queue. */
		for (q = TAILQ_FIRST(&deadq_head); q != NULL; q = TAILQ_NEXT(q, dq_entries))
			if (q->dq_pid == pid) {
				TAILQ_REMOVE(&deadq_head, q, dq_entries);
				free(q);
				goto oncemore;
			}

		/* Now, look in list of active processes. */
		for (f = Files; f; f = f->f_next)
			if (f->f_type == F_PIPE &&
			    f->f_un.f_pipe.f_pid == pid) {
				(void)close(f->f_file);

				errno = 0; /* Keep strerror() stuff out of logerror messages. */
				f->f_un.f_pipe.f_pid = 0;
				if (WIFSIGNALED(status)) {
					reason = "due to signal";
					code = WTERMSIG(status);
				} else {
					reason = "with status";
					code = WEXITSTATUS(status);
					if (code == 0)
						goto oncemore; /* Exited OK. */
				}
				(void)snprintf(buf, sizeof buf,
				"Logging subprocess %d (%s) exited %s %d.",
					       pid, f->f_un.f_pipe.f_pname,
					       reason, code);
				logerror(buf);
				break;
			}
	  oncemore:
	}
}

/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr_in *f;
{
	struct hostent *hp;
	char *p;

	dprintf("cvthname(%s)\n", inet_ntoa(f->sin_addr));

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address\n");
		return ("???");
	}
	hp = gethostbyaddr((char *)&f->sin_addr,
	    sizeof(struct in_addr), f->sin_family);
	if (hp == 0) {
		dprintf("Host name for your address (%s) unknown\n",
			inet_ntoa(f->sin_addr));
		return (inet_ntoa(f->sin_addr));
	}
	if ((p = strchr(hp->h_name, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hp->h_name);
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
			kill(q->dq_pid, SIGKILL);
			break;

		case 1:
			/*
			 * Timed out on dead queue, send terminate
			 * signal.  Note that we leave the removal
			 * from the dead queue to reapchild(), which
			 * will also log the event.
			 */
			kill(q->dq_pid, SIGTERM);
			/* FALLTROUGH */

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
	if (created_lsock)
		(void)unlink(LogName);
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
				deadq_enter(f->f_un.f_pipe.f_pid);
			f->f_un.f_pipe.f_pid = 0;
			break;
		}
		next = f->f_next;
		if(f->f_program) free(f->f_program);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.ERR\t/dev/console", *nextp, "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.PANIC\t*", (*nextp)->f_next, "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
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
		if(*p == '#') {
			p++;
			if(*p!='!')
				continue;
		}
		if(*p=='!') {
			p++;
			while(isspace(*p)) p++;
			if(!*p) {
				strcpy(prog, "*");
				continue;
			}
			for(i = 0; i < NAME_MAX; i++) {
				if(!isalnum(p[i]))
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
		cfline(cline, f, prog);
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
			if(f->f_program) {
				printf(" (%s)", f->f_program);
			}
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
cfline(line, f, prog)
	char *line;
	struct filed *f;
	char *prog;
{
	struct hostent *hp;
	int i, pri;
	char *bp, *p, *q;
	char buf[MAXLINE], ebuf[100];

	dprintf("cfline(\"%s\", f, \"%s\")\n", line, prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save program name if any */
	if(prog && *prog=='*') prog = NULL;
	if(prog) {
		f->f_program = calloc(1, strlen(prog)+1);
		if(f->f_program) {
			strcpy(f->f_program, prog);
		}
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
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
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
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
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t')
		p++;

	switch (*p)
	{
	case '@':
		(void)strcpy(f->f_un.f_forw.f_hname, ++p);
		hp = gethostbyname(p);
		if (hp == NULL) {
			extern int h_errno;

			logerror(hstrerror(h_errno));
			break;
		}
		memset(&f->f_un.f_forw.f_addr, 0,
			 sizeof(f->f_un.f_forw.f_addr));
		f->f_un.f_forw.f_addr.sin_family = AF_INET;
		f->f_un.f_forw.f_addr.sin_port = LogPort;
		memmove(&f->f_un.f_forw.f_addr.sin_addr, hp->h_addr, hp->h_length);
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
	regex_t re;
	int i;

	if ((cp1 = strrchr(s, ':'))) {
		/* service/port provided */
		*cp1++ = '\0';
		if (strlen(cp1) == 1 && *cp1 == '*')
			/* any port allowed */
			ap.port = htons(0);
		else if ((se = getservbyname(cp1, "udp")))
			ap.port = se->s_port;
		else {
			ap.port = htons((int)strtol(cp1, &cp2, 0));
			if (*cp2 != '\0')
				return -1; /* port not numeric */
		}
	} else {
		if ((se = getservbyname("syslog", "udp")))
			ap.port = se->s_port;
		else
			/* sanity, should not happen */
			ap.port = htons(514);
	}

	/* the regexp's are ugly, but the cleanest way */

	if (regcomp(&re, "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+(/[0-9]+)?$",
		    REG_EXTENDED))
		/* if RE compilation fails, that's an internal error */
		abort();
	if (regexec(&re, s, 0, 0, 0) == 0) {
		/* arg `s' is numeric */
		ap.isnumeric = 1;
		if ((cp1 = strchr(s, '/')) != NULL) {
			*cp1++ = '\0';
			i = atoi(cp1);
			if (i < 0 || i > 32)
				return -1;
			/* convert masklen to netmask */
			ap.a_mask.s_addr = htonl(~((1 << (32 - i)) - 1));
		}
		if (ascii2addr(AF_INET, s, &ap.a_addr) == -1)
			return -1;
		if (cp1 == NULL) {
			/* use default netmask */
			if (IN_CLASSA(ntohl(ap.a_addr.s_addr)))
				ap.a_mask.s_addr = htonl(IN_CLASSA_NET);
			else if (IN_CLASSB(ntohl(ap.a_addr.s_addr)))
				ap.a_mask.s_addr = htonl(IN_CLASSB_NET);
			else
				ap.a_mask.s_addr = htonl(IN_CLASSC_NET);
		}
	} else {
		/* arg `s' is domain name */
		ap.isnumeric = 0;
		ap.a_name = s;
	}
	regfree(&re);

	if (Debug) {
		printf("allowaddr: rule %d: ", NumAllowed);
		if (ap.isnumeric) {
			printf("numeric, ");
			printf("addr = %s, ",
			       addr2ascii(AF_INET, &ap.a_addr, sizeof(struct in_addr), 0));
			printf("mask = %s; ",
			       addr2ascii(AF_INET, &ap.a_mask, sizeof(struct in_addr), 0));
		} else
			printf("domainname = %s; ", ap.a_name);
		printf("port = %d\n", ntohs(ap.port));
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
validate(sin, hname)
	struct sockaddr_in *sin;
	const char *hname;
{
	int i;
	size_t l1, l2;
	char *cp, name[MAXHOSTNAMELEN];
	struct allowedpeer *ap;

	if (NumAllowed == 0)
		/* traditional behaviour, allow everything */
		return 1;

	strncpy(name, hname, sizeof name);
	if (strchr(name, '.') == NULL) {
		strncat(name, ".", sizeof name - strlen(name) - 1);
		strncat(name, LocalDomain, sizeof name - strlen(name) - 1);
	}
	dprintf("validate: dgram from IP %s, port %d, name %s;\n",
		addr2ascii(AF_INET, &sin->sin_addr, sizeof(struct in_addr), 0),
		ntohs(sin->sin_port), name);

	/* now, walk down the list */
	for (i = 0, ap = AllowedPeers; i < NumAllowed; i++, ap++) {
		if (ntohs(ap->port) != 0 && ap->port != sin->sin_port) {
			dprintf("rejected in rule %d due to port mismatch.\n", i);
			continue;
		}

		if (ap->isnumeric) {
			if ((sin->sin_addr.s_addr & ap->a_mask.s_addr)
			    != ap->a_addr.s_addr) {
				dprintf("rejected in rule %d due to IP mismatch.\n", i);
				continue;
			}
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

	mask = sigmask(SIGALRM) | sigmask(SIGHUP);
	sigprocmask(SIG_BLOCK, &omask, &mask);
	switch ((*pid = fork())) {
	case -1:
		sigprocmask(SIG_SETMASK, 0, &omask);
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
		sigprocmask(SIG_SETMASK, 0, &omask);
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

	sigprocmask(SIG_SETMASK, 0, &omask);
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
deadq_enter(pid)
	pid_t pid;
{
	dq_t p;

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
