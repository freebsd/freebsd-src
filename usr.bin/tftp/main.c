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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Command Interface.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/param.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <histedit.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define	MAXLINE		200
#define	TIMEOUT		5		/* secs between rexmt's */

struct	sockaddr_in peeraddr;
int	f;
short   port;
int	trace;
int	verbose;
int	connected;
char	mode[32];
char	line[MAXLINE];
int	margc;
char	*margv[20];
jmp_buf	toplevel;
struct	servent *sp;

void	get __P((int, char **));
void	help __P((int, char **));
void	intr __P((int));
void	modecmd __P((int, char **));
void	put __P((int, char **));
void	quit __P((int, char **));
void	setascii __P((int, char **));
void	setbinary __P((int, char **));
void	setpeer __P((int, char **));
void	setrexmt __P((int, char **));
void	settimeout __P((int, char **));
void	settrace __P((int, char **));
void	setverbose __P((int, char **));
void	status __P((int, char **));

static void command __P((void)) __dead2;
static const char *command_prompt __P((void));

static void getusage __P((char *));
static void makeargv __P((void));
static void putusage __P((char *));
static void settftpmode __P((const char *));

char	*tail __P((char *));
struct	cmd *getcmd __P((char *));

#define HELPINDENT (sizeof("connect"))

struct cmd {
	const char	*name;
	char	*help;
	void	(*handler) __P((int, char **));
};

char	vhelp[] = "toggle verbose mode";
char	thelp[] = "toggle packet tracing";
char	chelp[] = "connect to remote tftp";
char	qhelp[] = "exit tftp";
char	hhelp[] = "print help information";
char	shelp[] = "send file";
char	rhelp[] = "receive file";
char	mhelp[] = "set file transfer mode";
char	sthelp[] = "show current status";
char	xhelp[] = "set per-packet retransmission timeout";
char	ihelp[] = "set total retransmission timeout";
char    ashelp[] = "set mode to netascii";
char    bnhelp[] = "set mode to octet";

struct cmd cmdtab[] = {
	{ "connect",	chelp,		setpeer },
	{ "mode",       mhelp,          modecmd },
	{ "put",	shelp,		put },
	{ "get",	rhelp,		get },
	{ "quit",	qhelp,		quit },
	{ "verbose",	vhelp,		setverbose },
	{ "trace",	thelp,		settrace },
	{ "status",	sthelp,		status },
	{ "binary",     bnhelp,         setbinary },
	{ "ascii",      ashelp,         setascii },
	{ "rexmt",	xhelp,		setrexmt },
	{ "timeout",	ihelp,		settimeout },
	{ "?",		hhelp,		help },
	{ NULL, NULL, NULL }
};

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct sockaddr_in lsin;

	sp = getservbyname("tftp", "udp");
	if (sp == 0)
		errx(1, "udp/tftp: unknown service");
	f = socket(AF_INET, SOCK_DGRAM, 0);
	if (f < 0)
		err(3, "socket");
	bzero((char *)&lsin, sizeof(lsin));
	lsin.sin_family = AF_INET;
	if (bind(f, (struct sockaddr *)&lsin, sizeof(lsin)) < 0)
		err(1, "bind");
	strcpy(mode, "netascii");
	signal(SIGINT, intr);
	if (argc > 1) {
		if (setjmp(toplevel) != 0)
			exit(0);
		setpeer(argc, argv);
	}
	if (setjmp(toplevel) != 0)
		(void)putchar('\n');
	command();
}

char    hostname[MAXHOSTNAMELEN];

void
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	struct hostent *host;

	if (argc < 2) {
		strcpy(line, "Connect ");
		printf("(to) ");
		fgets(&line[strlen(line)], sizeof line - strlen(line), stdin);
		makeargv();
		argc = margc;
		argv = margv;
	}
	if ((argc < 2) || (argv > 3)) {
		printf("usage: %s host-name [port]\n", argv[0]);
		return;
	}
	host = gethostbyname(argv[1]);
	if (host) {
		peeraddr.sin_family = host->h_addrtype;
		bcopy(host->h_addr, &peeraddr.sin_addr, 
			MIN(sizeof(peeraddr.sin_addr), host->h_length));
		strncpy(hostname, host->h_name, sizeof(hostname));
	} else {
		peeraddr.sin_family = AF_INET;
		peeraddr.sin_addr.s_addr = inet_addr(argv[1]);
		if (peeraddr.sin_addr.s_addr == -1) {
			connected = 0;
			printf("%s: unknown host\n", argv[1]);
			return;
		}
		strncpy(hostname, argv[1], sizeof(hostname));
	}
	hostname[sizeof(hostname) - 1] = '\0';
	port = sp->s_port;
	if (argc == 3) {
		port = atoi(argv[2]);
		if (port < 0) {
			printf("%s: bad port number\n", argv[2]);
			connected = 0;
			return;
		}
		port = htons(port);
	}
	connected = 1;
}

struct	modes {
	const char *m_name;
	const char *m_mode;
} modes[] = {
	{ "ascii",	"netascii" },
	{ "netascii",   "netascii" },
	{ "binary",     "octet" },
	{ "image",      "octet" },
	{ "octet",     "octet" },
/*      { "mail",       "mail" },       */
	{ 0,		0 }
};

void
modecmd(argc, argv)
	int argc;
	char *argv[];
{
	struct modes *p;
	const char *sep;

	if (argc < 2) {
		printf("Using %s mode to transfer files.\n", mode);
		return;
	}
	if (argc == 2) {
		for (p = modes; p->m_name; p++)
			if (strcmp(argv[1], p->m_name) == 0)
				break;
		if (p->m_name) {
			settftpmode(p->m_mode);
			return;
		}
		printf("%s: unknown mode\n", argv[1]);
		/* drop through and print usage message */
	}

	printf("usage: %s [", argv[0]);
	sep = " ";
	for (p = modes; p->m_name; p++) {
		printf("%s%s", sep, p->m_name);
		if (*sep == ' ')
			sep = " | ";
	}
	printf(" ]\n");
	return;
}

void
setbinary(argc, argv)
	int argc __unused;
	char *argv[] __unused;
{

	settftpmode("octet");
}

void
setascii(argc, argv)
	int argc __unused;
	char *argv[] __unused;
{

	settftpmode("netascii");
}

static void
settftpmode(newmode)
	const char *newmode;
{
	strcpy(mode, newmode);
	if (verbose)
		printf("mode set to %s\n", mode);
}


/*
 * Send file(s).
 */
void
put(argc, argv)
	int argc;
	char *argv[];
{
	int fd;
	int n;
	char *cp, *targ;

	if (argc < 2) {
		strcpy(line, "send ");
		printf("(file) ");
		fgets(&line[strlen(line)], sizeof line - strlen(line), stdin);
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		putusage(argv[0]);
		return;
	}
	targ = argv[argc - 1];
	if (index(argv[argc - 1], ':')) {
		char *lcp;
		struct hostent *hp;

		for (n = 1; n < argc - 1; n++)
			if (index(argv[n], ':')) {
				putusage(argv[0]);
				return;
			}
		lcp = argv[argc - 1];
		targ = index(lcp, ':');
		*targ++ = 0;
		hp = gethostbyname(lcp);
		if (hp == NULL) {
			fprintf(stderr, "tftp: %s: ", lcp);
			herror((char *)NULL);
			return;
		}
		bcopy(hp->h_addr, (caddr_t)&peeraddr.sin_addr, 
			MIN(sizeof(peeraddr.sin_addr), hp->h_length));
		peeraddr.sin_family = hp->h_addrtype;
		connected = 1;
		strncpy(hostname, hp->h_name, sizeof(hostname));
		hostname[sizeof(hostname) - 1] = '\0';
	}
	if (!connected) {
		printf("No target machine specified.\n");
		return;
	}
	if (argc < 4) {
		cp = argc == 2 ? tail(targ) : argv[1];
		fd = open(cp, O_RDONLY);
		if (fd < 0) {
			warn("%s", cp);
			return;
		}
		if (verbose)
			printf("putting %s to %s:%s [%s]\n",
				cp, hostname, targ, mode);
		peeraddr.sin_port = port;
		xmitfile(fd, targ, mode);
		return;
	}
				/* this assumes the target is a directory */
				/* on a remote unix system.  hmmmm.  */
	cp = index(targ, '\0');
	*cp++ = '/';
	for (n = 1; n < argc - 1; n++) {
		strcpy(cp, tail(argv[n]));
		fd = open(argv[n], O_RDONLY);
		if (fd < 0) {
			warn("%s", argv[n]);
			continue;
		}
		if (verbose)
			printf("putting %s to %s:%s [%s]\n",
				argv[n], hostname, targ, mode);
		peeraddr.sin_port = port;
		xmitfile(fd, targ, mode);
	}
}

static void
putusage(s)
	char *s;
{
	printf("usage: %s file ... host:target, or\n", s);
	printf("       %s file ... target (when already connected)\n", s);
}

/*
 * Receive file(s).
 */
void
get(argc, argv)
	int argc;
	char *argv[];
{
	int fd;
	int n;
	char *cp;
	char *src;

	if (argc < 2) {
		strcpy(line, "get ");
		printf("(files) ");
		fgets(&line[strlen(line)], sizeof line - strlen(line), stdin);
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		getusage(argv[0]);
		return;
	}
	if (!connected) {
		for (n = 1; n < argc ; n++)
			if (index(argv[n], ':') == 0) {
				getusage(argv[0]);
				return;
			}
	}
	for (n = 1; n < argc ; n++) {
		src = index(argv[n], ':');
		if (src == NULL)
			src = argv[n];
		else {
			struct hostent *hp;

			*src++ = 0;
			hp = gethostbyname(argv[n]);
			if (hp == NULL) {
				fprintf(stderr, "tftp: %s: ", argv[n]);
				herror((char *)NULL);
				continue;
			}
			bcopy(hp->h_addr, (caddr_t)&peeraddr.sin_addr,
			    MIN(sizeof(peeraddr.sin_addr), hp->h_length));
			peeraddr.sin_family = hp->h_addrtype;
			connected = 1;
			strncpy(hostname, hp->h_name, sizeof(hostname));
			hostname[sizeof(hostname) - 1] = '\0';
		}
		if (argc < 4) {
			cp = argc == 3 ? argv[2] : tail(src);
			fd = creat(cp, 0644);
			if (fd < 0) {
				warn("%s", cp);
				return;
			}
			if (verbose)
				printf("getting from %s:%s to %s [%s]\n",
					hostname, src, cp, mode);
			peeraddr.sin_port = port;
			recvfile(fd, src, mode);
			break;
		}
		cp = tail(src);         /* new .. jdg */
		fd = creat(cp, 0644);
		if (fd < 0) {
			warn("%s", cp);
			continue;
		}
		if (verbose)
			printf("getting from %s:%s to %s [%s]\n",
				hostname, src, cp, mode);
		peeraddr.sin_port = port;
		recvfile(fd, src, mode);
	}
}

static void
getusage(s)
	char *s;
{
	printf("usage: %s host:file host:file ... file, or\n", s);
	printf("       %s file file ... file if connected\n", s);
}

int	rexmtval = TIMEOUT;

void
setrexmt(argc, argv)
	int argc;
	char *argv[];
{
	int t;

	if (argc < 2) {
		strcpy(line, "Rexmt-timeout ");
		printf("(value) ");
		fgets(&line[strlen(line)], sizeof line - strlen(line), stdin);
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = atoi(argv[1]);
	if (t < 0)
		printf("%s: bad value\n", argv[1]);
	else
		rexmtval = t;
}

int	maxtimeout = 5 * TIMEOUT;

void
settimeout(argc, argv)
	int argc;
	char *argv[];
{
	int t;

	if (argc < 2) {
		strcpy(line, "Maximum-timeout ");
		printf("(value) ");
		fgets(&line[strlen(line)], sizeof line - strlen(line), stdin);
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = atoi(argv[1]);
	if (t < 0)
		printf("%s: bad value\n", argv[1]);
	else
		maxtimeout = t;
}

void
status(argc, argv)
	int argc __unused;
	char *argv[] __unused;
{
	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	printf("Mode: %s Verbose: %s Tracing: %s\n", mode,
		verbose ? "on" : "off", trace ? "on" : "off");
	printf("Rexmt-interval: %d seconds, Max-timeout: %d seconds\n",
		rexmtval, maxtimeout);
}

void
intr(dummy)
	int dummy __unused;
{

	signal(SIGALRM, SIG_IGN);
	alarm(0);
	longjmp(toplevel, -1);
}

char *
tail(filename)
	char *filename;
{
	char *s;

	while (*filename) {
		s = rindex(filename, '/');
		if (s == NULL)
			break;
		if (s[1])
			return (s + 1);
		*s = '\0';
	}
	return (filename);
}

static const char *
command_prompt()
{

	return ("tftp> ");
}

/*
 * Command parser.
 */
static void
command()
{
	HistEvent he;
	struct cmd *c;
	static EditLine *el;
	static History *hist;
	const char *bp;
	char *cp;
	int len, num, vrbose;

	vrbose = isatty(0);
	if (vrbose) {
		el = el_init("tftp", stdin, stdout, stderr);
		hist = history_init();
		history(hist, &he, H_EVENT, 100);
		el_set(el, EL_HIST, history, hist);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_PROMPT, command_prompt);
		el_set(el, EL_SIGNAL, 1);
		el_source(el, NULL);
	}
	for (;;) {
		if (vrbose) {
                        if ((bp = el_gets(el, &num)) == NULL || num == 0)
                                exit(0);
                        len = (num > MAXLINE) ? MAXLINE : num;
                        memcpy(line, bp, len);
                        line[len] = '\0';
                        history(hist, &he, H_ENTER, bp);
		} else {
			if (fgets(line, sizeof line , stdin) == 0) {
				if (feof(stdin)) {
					exit(0);
				} else {
					continue;
				}
			}
		}
		if ((cp = strchr(line, '\n')))
			*cp = '\0';
		if (line[0] == 0)
			continue;
		makeargv();
		if (margc == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			printf("?Invalid command\n");
			continue;
		}
		(*c->handler)(margc, margv);
	}
}

struct cmd *
getcmd(name)
	char *name;
{
	const char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->name) != NULL; c++) {
		for (q = name; *q == *p++; q++)
			if (*q == 0)		/* exact match? */
				return (c);
		if (!*q) {			/* the name was a prefix */
			if (q - name > longest) {
				longest = q - name;
				nmatches = 1;
				found = c;
			} else if (q - name == longest)
				nmatches++;
		}
	}
	if (nmatches > 1)
		return ((struct cmd *)-1);
	return (found);
}

/*
 * Slice a string up into argc/argv.
 */
static void
makeargv()
{
	char *cp;
	char **argp = margv;

	margc = 0;
	if ((cp = strchr(line, '\n')))
		*cp = '\0';
	for (cp = line; *cp;) {
		while (isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*argp++ = cp;
		margc += 1;
		while (*cp != '\0' && !isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*cp++ = '\0';
	}
	*argp++ = 0;
}

void
quit(argc, argv)
	int argc __unused;
	char *argv[] __unused;
{

	exit(0);
}

/*
 * Help command.
 */
void
help(argc, argv)
	int argc;
	char *argv[];
{
	struct cmd *c;

	if (argc == 1) {
		printf("Commands may be abbreviated.  Commands are:\n\n");
		for (c = cmdtab; c->name; c++)
			printf("%-*s\t%s\n", (int)HELPINDENT, c->name, c->help);
		return;
	}
	while (--argc > 0) {
		char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%s\n", c->help);
	}
}

void
settrace(argc, argv)
	int argc __unused;
	char **argv __unused;
{
	trace = !trace;
	printf("Packet tracing %s.\n", trace ? "on" : "off");
}

void
setverbose(argc, argv)
	int argc __unused;
	char **argv __unused;
{
	verbose = !verbose;
	printf("Verbose mode %s.\n", verbose ? "on" : "off");
}
