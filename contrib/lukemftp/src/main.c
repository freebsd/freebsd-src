/*	$NetBSD: main.c,v 1.90 2004/07/21 00:09:14 lukem Exp $	*/

/*-
 * Copyright (c) 1996-2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1985, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 10/9/94";
#else
__RCSID("$NetBSD: main.c,v 1.90 2004/07/21 00:09:14 lukem Exp $");
#endif
#endif /* not lint */

/*
 * FTP User Program -- Command Interface.
 */
#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#define	GLOBAL		/* force GLOBAL decls in ftp_var.h to be declared */
#include "ftp_var.h"

#define	FTP_PROXY	"ftp_proxy"	/* env var with FTP proxy location */
#define	HTTP_PROXY	"http_proxy"	/* env var with HTTP proxy location */
#define	NO_PROXY	"no_proxy"	/* env var with list of non-proxied
					 * hosts, comma or space separated */

static void	setupoption(char *, char *, char *);
int		main(int, char *[]);

int
main(int argc, char *argv[])
{
	int ch, rval;
	struct passwd *pw;
	char *cp, *ep, *anonuser, *anonpass, *upload_path;
	int dumbterm, s, len, isupload;

	setlocale(LC_ALL, "");
	setprogname(argv[0]);

	sigint_raised = 0;

	ftpport = "ftp";
	httpport = "http";
	gateport = NULL;
	cp = getenv("FTPSERVERPORT");
	if (cp != NULL)
		gateport = cp;
	else
		gateport = "ftpgate";
	doglob = 1;
	interactive = 1;
	autologin = 1;
	passivemode = 1;
	activefallback = 1;
	preserve = 1;
	verbose = 0;
	progress = 0;
	gatemode = 0;
	data = -1;
	outfile = NULL;
	restartautofetch = 0;
#ifndef NO_EDITCOMPLETE
	editing = 0;
	el = NULL;
	hist = NULL;
#endif
	bytes = 0;
	mark = HASHBYTES;
	rate_get = 0;
	rate_get_incr = DEFAULTINCR;
	rate_put = 0;
	rate_put_incr = DEFAULTINCR;
#ifdef INET6
	epsv4 = 1;
#else
	epsv4 = 0;
#endif
	epsv4bad = 0;
	upload_path = NULL;
	isupload = 0;
	reply_callback = NULL;
	family = AF_UNSPEC;

	netrc[0] = '\0';
	cp = getenv("NETRC");
	if (cp != NULL && strlcpy(netrc, cp, sizeof(netrc)) >= sizeof(netrc))
		errx(1, "$NETRC `%s': %s", cp, strerror(ENAMETOOLONG));

	/*
	 * Get the default socket buffer sizes if we don't already have them.
	 * It doesn't matter which socket we do this to, because on the first
	 * call no socket buffer sizes will have been modified, so we are
	 * guaranteed to get the system defaults.
	 */
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "can't create socket");
	len = sizeof(rcvbuf_size);
	if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, (void *) &rcvbuf_size, &len)
	    < 0)
		err(1, "unable to get default rcvbuf size");
	len = sizeof(sndbuf_size);
	if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *) &sndbuf_size, &len)
	    < 0)
		err(1, "unable to get default sndbuf size");
	(void)close(s);
					/* sanity check returned buffer sizes */
	if (rcvbuf_size <= 0)
		rcvbuf_size = 8 * 1024;
	if (sndbuf_size <= 0)
		sndbuf_size = 8 * 1024;

	if (sndbuf_size > 8 * 1024 * 1024)
		sndbuf_size = 8 * 1024 * 1024;
	if (rcvbuf_size > 8 * 1024 * 1024)
		rcvbuf_size = 8 * 1024 * 1024;

	marg_sl = xsl_init();
	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;

	/* Set default operation mode based on FTPMODE environment variable */
	if ((cp = getenv("FTPMODE")) != NULL) {
		if (strcasecmp(cp, "passive") == 0) {
			passivemode = 1;
			activefallback = 0;
		} else if (strcasecmp(cp, "active") == 0) {
			passivemode = 0;
			activefallback = 0;
		} else if (strcasecmp(cp, "gate") == 0) {
			gatemode = 1;
		} else if (strcasecmp(cp, "auto") == 0) {
			passivemode = 1;
			activefallback = 1;
		} else
			warnx("unknown $FTPMODE '%s'; using defaults", cp);
	}

	if (strcmp(getprogname(), "pftp") == 0) {
		passivemode = 1;
		activefallback = 0;
	} else if (strcmp(getprogname(), "gate-ftp") == 0)
		gatemode = 1;

	gateserver = getenv("FTPSERVER");
	if (gateserver == NULL || *gateserver == '\0')
		gateserver = GATE_SERVER;
	if (gatemode) {
		if (*gateserver == '\0') {
			warnx(
"Neither $FTPSERVER nor GATE_SERVER is defined; disabling gate-ftp");
			gatemode = 0;
		}
	}

	cp = getenv("TERM");
	if (cp == NULL || strcmp(cp, "dumb") == 0)
		dumbterm = 1;
	else
		dumbterm = 0;
	fromatty = isatty(fileno(stdin));
	ttyout = stdout;
	if (isatty(fileno(ttyout))) {
		verbose = 1;		/* verbose if to a tty */
		if (! dumbterm) {
#ifndef NO_EDITCOMPLETE
			if (fromatty)	/* editing mode on if tty is usable */
				editing = 1;
#endif
#ifndef NO_PROGRESS
			if (foregroundproc())
				progress = 1;	/* progress bar on if fg */
#endif
		}
	}

	while ((ch = getopt(argc, argv, "46AadefginN:o:pP:q:r:RtT:u:vV")) != -1) {
		switch (ch) {
		case '4':
			family = AF_INET;
			break;

		case '6':
#ifdef INET6
			family = AF_INET6;
#else
			warnx("INET6 support is not available; ignoring -6");
#endif
			break;

		case 'A':
			activefallback = 0;
			passivemode = 0;
			break;

		case 'a':
			anonftp = 1;
			break;

		case 'd':
			options |= SO_DEBUG;
			debug++;
			break;

		case 'e':
#ifndef NO_EDITCOMPLETE
			editing = 0;
#endif
			break;

		case 'f':
			flushcache = 1;
			break;

		case 'g':
			doglob = 0;
			break;

		case 'i':
			interactive = 0;
			break;

		case 'n':
			autologin = 0;
			break;

		case 'N':
			if (strlcpy(netrc, optarg, sizeof(netrc))
			    >= sizeof(netrc))
				errx(1, "%s: %s", optarg,
				    strerror(ENAMETOOLONG));
			break;

		case 'o':
			outfile = optarg;
			if (strcmp(outfile, "-") == 0)
				ttyout = stderr;
			break;

		case 'p':
			passivemode = 1;
			activefallback = 0;
			break;

		case 'P':
			ftpport = optarg;
			break;

		case 'q':
			quit_time = strtol(optarg, &ep, 10);
			if (quit_time < 1 || *ep != '\0')
				errx(1, "bad quit value: %s", optarg);
			break;

		case 'r':
			retry_connect = strtol(optarg, &ep, 10);
			if (retry_connect < 1 || *ep != '\0')
				errx(1, "bad retry value: %s", optarg);
			break;

		case 'R':
			restartautofetch = 1;
			break;

		case 't':
			trace = 1;
			break;

		case 'T':
		{
			int targc;
			char *targv[6], *oac;

				/* look for `dir,max[,incr]' */
			targc = 0;
			targv[targc++] = "-T";
			oac = xstrdup(optarg);

			while ((cp = strsep(&oac, ",")) != NULL) {
				if (*cp == '\0') {
					warnx("bad throttle value: %s", optarg);
					usage();
					/* NOTREACHED */
				}
				targv[targc++] = cp;
				if (targc >= 5)
					break;
			}
			if (parserate(targc, targv, 1) == -1)
				usage();
			free(oac);
			break;
		}

		case 'u':
		{
			isupload = 1;
			interactive = 0;
			upload_path = xstrdup(optarg);

			break;
		}

		case 'v':
			progress = verbose = 1;
			break;

		case 'V':
			progress = verbose = 0;
			break;

		default:
			usage();
		}
	}
			/* set line buffering on ttyout */
	setvbuf(ttyout, NULL, _IOLBF, 0);
	argc -= optind;
	argv += optind;

	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
	crflag = 1;	/* strip c.r. on ascii gets */
	sendport = -1;	/* not using ports */

	/*
	 * Cache the user name and home directory.
	 */
	localhome = NULL;
	localname = NULL;
	anonuser = "anonymous";
	cp = getenv("HOME");
	if (! EMPTYSTRING(cp))
		localhome = xstrdup(cp);
	pw = NULL;
	cp = getlogin();
	if (cp != NULL)
		pw = getpwnam(cp);
	if (pw == NULL)
		pw = getpwuid(getuid());
	if (pw != NULL) {
		if (localhome == NULL && !EMPTYSTRING(pw->pw_dir))
			localhome = xstrdup(pw->pw_dir);
		localname = xstrdup(pw->pw_name);
		anonuser = localname;
	}
	if (netrc[0] == '\0' && localhome != NULL) {
		if (strlcpy(netrc, localhome, sizeof(netrc)) >= sizeof(netrc) ||
	    	    strlcat(netrc, "/.netrc", sizeof(netrc)) >= sizeof(netrc)) {
			warnx("%s/.netrc: %s", localhome,
			    strerror(ENAMETOOLONG));
			netrc[0] = '\0';
		}
	}
	if (localhome == NULL)
		localhome = xstrdup("/");

	/*
	 * Every anonymous FTP server I've encountered will accept the
	 * string "username@", and will append the hostname itself. We
	 * do this by default since many servers are picky about not
	 * having a FQDN in the anonymous password.
	 * - thorpej@NetBSD.org
	 */
	len = strlen(anonuser) + 2;
	anonpass = xmalloc(len);
	(void)strlcpy(anonpass, anonuser, len);
	(void)strlcat(anonpass, "@",	  len);

			/*
			 * set all the defaults for options defined in
			 * struct option optiontab[]  declared in cmdtab.c
			 */
	setupoption("anonpass",		getenv("FTPANONPASS"),	anonpass);
	setupoption("ftp_proxy",	getenv(FTP_PROXY),	"");
	setupoption("http_proxy",	getenv(HTTP_PROXY),	"");
	setupoption("no_proxy",		getenv(NO_PROXY),	"");
	setupoption("pager",		getenv("PAGER"),	DEFAULTPAGER);
	setupoption("prompt",		getenv("FTPPROMPT"),	DEFAULTPROMPT);
	setupoption("rprompt",		getenv("FTPRPROMPT"),	DEFAULTRPROMPT);

	free(anonpass);

	setttywidth(0);
#ifdef SIGINFO
	(void)xsignal(SIGINFO, psummary);
#endif
	(void)xsignal(SIGQUIT, psummary);
	(void)xsignal(SIGUSR1, crankrate);
	(void)xsignal(SIGUSR2, crankrate);
	(void)xsignal(SIGWINCH, setttywidth);

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&argc;
	(void)&argv;
#endif

	if (argc > 0) {
		if (isupload) {
			rval = auto_put(argc, argv, upload_path);
 sigint_or_rval_exit:
			if (sigint_raised) {
				(void)xsignal(SIGINT, SIG_DFL);
				raise(SIGINT);
			}
			exit(rval);
		} else if (strchr(argv[0], ':') != NULL
			    && ! isipv6addr(argv[0])) {
			rval = auto_fetch(argc, argv);
			if (rval >= 0)		/* -1 == connected and cd-ed */
				goto sigint_or_rval_exit;
		} else {
			char *xargv[4], *user, *host;

			if ((rval = sigsetjmp(toplevel, 1)))
				goto sigint_or_rval_exit;
			(void)xsignal(SIGINT, intr);
			(void)xsignal(SIGPIPE, lostpeer);
			user = NULL;
			host = argv[0];
			cp = strchr(host, '@');
			if (cp) {
				*cp = '\0';
				user = host;
				host = cp + 1;
			}
			/* XXX discards const */
			xargv[0] = (char *)getprogname();
			xargv[1] = host;
			xargv[2] = argv[1];
			xargv[3] = NULL;
			do {
				int oautologin;

				oautologin = autologin;
				if (user != NULL) {
					anonftp = 0;
					autologin = 0;
				}
				setpeer(argc+1, xargv);
				autologin = oautologin;
				if (connected == 1 && user != NULL)
					(void)ftp_login(host, user, NULL);
				if (!retry_connect)
					break;
				if (!connected) {
					macnum = 0;
					fprintf(ttyout,
					    "Retrying in %d seconds...\n",
					    retry_connect);
					sleep(retry_connect);
				}
			} while (!connected);
			retry_connect = 0; /* connected, stop hiding msgs */
		}
	}
	if (isupload)
		usage();

#ifndef NO_EDITCOMPLETE
	controlediting();
#endif /* !NO_EDITCOMPLETE */

	(void)sigsetjmp(toplevel, 1);
	(void)xsignal(SIGINT, intr);
	(void)xsignal(SIGPIPE, lostpeer);
	for (;;)
		cmdscanner();
}

/*
 * Generate a prompt
 */
char *
prompt(void)
{
	static char	**prompt;
	static char	  buf[MAXPATHLEN];

	if (prompt == NULL) {
		struct option *o;

		o = getoption("prompt");
		if (o == NULL)
			errx(1, "no such option `prompt'");
		prompt = &(o->value);
	}
	formatbuf(buf, sizeof(buf), *prompt ? *prompt : DEFAULTPROMPT);
	return (buf);
}

/*
 * Generate an rprompt
 */
char *
rprompt(void)
{
	static char	**rprompt;
	static char	  buf[MAXPATHLEN];

	if (rprompt == NULL) {
		struct option *o;

		o = getoption("rprompt");
		if (o == NULL)
			errx(1, "no such option `rprompt'");
		rprompt = &(o->value);
	}
	formatbuf(buf, sizeof(buf), *rprompt ? *rprompt : DEFAULTRPROMPT);
	return (buf);
}

/*
 * Command parser.
 */
void
cmdscanner(void)
{
	struct cmd	*c;
	char		*p;
	int		 num;

	for (;;) {
#ifndef NO_EDITCOMPLETE
		if (!editing) {
#endif /* !NO_EDITCOMPLETE */
			if (fromatty) {
				fputs(prompt(), ttyout);
				p = rprompt();
				if (*p)
					fprintf(ttyout, "%s ", p);
				(void)fflush(ttyout);
			}
			if (fgets(line, sizeof(line), stdin) == NULL) {
				if (fromatty)
					putc('\n', ttyout);
				quit(0, NULL);
			}
			num = strlen(line);
			if (num == 0)
				break;
			if (line[--num] == '\n') {
				if (num == 0)
					break;
				line[num] = '\0';
			} else if (num == sizeof(line) - 2) {
				fputs("sorry, input line too long.\n", ttyout);
				while ((num = getchar()) != '\n' && num != EOF)
					/* void */;
				break;
			} /* else it was a line without a newline */
#ifndef NO_EDITCOMPLETE
		} else {
			const char *buf;
			HistEvent ev;
			cursor_pos = NULL;

			if ((buf = el_gets(el, &num)) == NULL || num == 0) {
				if (fromatty)
					putc('\n', ttyout);
				quit(0, NULL);
			}
			if (buf[--num] == '\n') {
				if (num == 0)
					break;
			} else if (num >= sizeof(line)) {
				fputs("sorry, input line too long.\n", ttyout);
				break;
			}
			memcpy(line, buf, num);
			line[num] = '\0';
			history(hist, &ev, H_ENTER, buf);
		}
#endif /* !NO_EDITCOMPLETE */

		makeargv();
		if (margc == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			fputs("?Ambiguous command.\n", ttyout);
			continue;
		}
		if (c == NULL) {
#if !defined(NO_EDITCOMPLETE)
			/*
			 * attempt to el_parse() unknown commands.
			 * any command containing a ':' would be parsed
			 * as "[prog:]cmd ...", and will result in a
			 * false positive if prog != "ftp", so treat
			 * such commands as invalid.
			 */
			if (strchr(margv[0], ':') != NULL ||
			    el_parse(el, margc, (const char **)margv) != 0)
#endif /* !NO_EDITCOMPLETE */
				fputs("?Invalid command.\n", ttyout);
			continue;
		}
		if (c->c_conn && !connected) {
			fputs("Not connected.\n", ttyout);
			continue;
		}
		confirmrest = 0;
		margv[0] = c->c_name;
		(*c->c_handler)(margc, margv);
		if (bell && c->c_bell)
			(void)putc('\007', ttyout);
		if (c->c_handler != help)
			break;
	}
	(void)xsignal(SIGINT, intr);
	(void)xsignal(SIGPIPE, lostpeer);
}

struct cmd *
getcmd(const char *name)
{
	const char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	if (name == NULL)
		return (0);

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->c_name) != NULL; c++) {
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

int slrflag;

void
makeargv(void)
{
	char *argp;

	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	marg_sl->sl_cur = 0;		/* reset to start of marg_sl */
	for (margc = 0; ; margc++) {
		argp = slurpstring();
		xsl_add(marg_sl, argp);
		if (argp == NULL)
			break;
	}
#ifndef NO_EDITCOMPLETE
	if (cursor_pos == line) {
		cursor_argc = 0;
		cursor_argo = 0;
	} else if (cursor_pos != NULL) {
		cursor_argc = margc;
		cursor_argo = strlen(margv[margc-1]);
	}
#endif /* !NO_EDITCOMPLETE */
}

#ifdef NO_EDITCOMPLETE
#define	INC_CHKCURSOR(x)	(x)++
#else  /* !NO_EDITCOMPLETE */
#define	INC_CHKCURSOR(x)	{ (x)++ ; \
				if (x == cursor_pos) { \
					cursor_argc = margc; \
					cursor_argo = ap-argbase; \
					cursor_pos = NULL; \
				} }

#endif /* !NO_EDITCOMPLETE */

/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
char *
slurpstring(void)
{
	int got_one = 0;
	char *sb = stringbase;
	char *ap = argbase;
	char *tmp = argbase;		/* will return this if token found */

	if (*sb == '!' || *sb == '$') {	/* recognize ! as a token for shell */
		switch (slrflag) {	/* and $ as token for macro invoke */
			case 0:
				slrflag++;
				INC_CHKCURSOR(stringbase);
				return ((*sb == '!') ? "!" : "$");
				/* NOTREACHED */
			case 1:
				slrflag++;
				altarg = stringbase;
				break;
			default:
				break;
		}
	}

S0:
	switch (*sb) {

	case '\0':
		goto OUT;

	case ' ':
	case '\t':
		INC_CHKCURSOR(sb);
		goto S0;

	default:
		switch (slrflag) {
			case 0:
				slrflag++;
				break;
			case 1:
				slrflag++;
				altarg = sb;
				break;
			default:
				break;
		}
		goto S1;
	}

S1:
	switch (*sb) {

	case ' ':
	case '\t':
	case '\0':
		goto OUT;	/* end of token */

	case '\\':
		INC_CHKCURSOR(sb);
		goto S2;	/* slurp next character */

	case '"':
		INC_CHKCURSOR(sb);
		goto S3;	/* slurp quoted string */

	default:
		*ap = *sb;	/* add character to token */
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		INC_CHKCURSOR(sb);
		goto S1;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S3;
	}

OUT:
	if (got_one)
		*ap++ = '\0';
	argbase = ap;			/* update storage pointer */
	stringbase = sb;		/* update scan pointer */
	if (got_one) {
		return (tmp);
	}
	switch (slrflag) {
		case 0:
			slrflag++;
			break;
		case 1:
			slrflag++;
			altarg = NULL;
			break;
		default:
			break;
	}
	return (NULL);
}

/*
 * Help/usage command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help(int argc, char *argv[])
{
	struct cmd *c;
	char *nargv[1], *p, *cmd;
	int isusage;

	cmd = argv[0];
	isusage = (strcmp(cmd, "usage") == 0);
	if (argc == 0 || (isusage && argc == 1)) {
		fprintf(ttyout, "usage: %s [command [...]]\n", cmd);
		return;
	}
	if (argc == 1) {
		StringList *buf;

		buf = xsl_init();
		fprintf(ttyout,
		    "%sommands may be abbreviated.  Commands are:\n\n",
		    proxy ? "Proxy c" : "C");
		for (c = cmdtab; (p = c->c_name) != NULL; c++)
			if (!proxy || c->c_proxy)
				xsl_add(buf, p);
		list_vertical(buf);
		sl_free(buf, 0);
		return;
	}

#define	HELPINDENT ((int) sizeof("disconnect"))

	while (--argc > 0) {
		char *arg;

		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			fprintf(ttyout, "?Ambiguous %s command `%s'\n",
			    cmd, arg);
		else if (c == NULL)
			fprintf(ttyout, "?Invalid %s command `%s'\n",
			    cmd, arg);
		else {
			if (isusage) {
				nargv[0] = c->c_name;
				(*c->c_handler)(0, nargv);
			} else
				fprintf(ttyout, "%-*s\t%s\n", HELPINDENT,
				    c->c_name, c->c_help);
		}
	}
}

struct option *
getoption(const char *name)
{
	const char *p;
	struct option *c;

	if (name == NULL)
		return (NULL);
	for (c = optiontab; (p = c->name) != NULL; c++) {
		if (strcasecmp(p, name) == 0)
			return (c);
	}
	return (NULL);
}

char *
getoptionvalue(const char *name)
{
	struct option *c;

	if (name == NULL)
		errx(1, "getoptionvalue() invoked with NULL name");
	c = getoption(name);
	if (c != NULL)
		return (c->value);
	errx(1, "getoptionvalue() invoked with unknown option `%s'", name);
	/* NOTREACHED */
}

static void
setupoption(char *name, char *value, char *defaultvalue)
{
	char *nargv[3];
	int overbose;

	nargv[0] = "setupoption()";
	nargv[1] = name;
	nargv[2] = (value ? value : defaultvalue);
	overbose = verbose;
	verbose = 0;
	setoption(3, nargv);
	verbose = overbose;
}

void
usage(void)
{
	const char *progname = getprogname();

	(void)fprintf(stderr,
"usage: %s [-46AadefginpRtvV] [-N netrc] [-o outfile] [-P port] [-q quittime]\n"
"           [-r retry] [-T dir,max[,inc][[user@]host [port]]] [host:path[/]]\n"
"           [file:///file] [ftp://[user[:pass]@]host[:port]/path[/]]\n"
"           [http://[user[:pass]@]host[:port]/path] [...]\n"
"       %s -u URL file [...]\n", progname, progname);
	exit(1);
}
