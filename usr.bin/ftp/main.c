/*	$Id: main.c,v 1.16 1997/12/13 20:38:19 pst Exp $	*/
/*	$NetBSD: main.c,v 1.26 1997/10/14 16:31:22 christos Exp $	*/

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
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1985, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 10/9/94";
#else
__RCSID("$Id: main.c,v 1.16 1997/12/13 20:38:19 pst Exp $");
__RCSID_SOURCE("$NetBSD: main.c,v 1.26 1997/10/14 16:31:22 christos Exp $");
#endif
#endif /* not lint */

/*
 * FTP User Program -- Command Interface.
 */
#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp_var.h"
#include "pathnames.h"

int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct servent *sp;
	int ch, top, rval;
	long port;
	struct passwd *pw = NULL;
	char *cp, *ep, homedir[MAXPATHLEN];
	int dumbterm;

	(void) setlocale(LC_ALL, "");

	sp = getservbyname("ftp", "tcp");
	if (sp == 0)
		ftpport = htons(FTP_PORT);	/* good fallback */
	else
		ftpport = sp->s_port;
	sp = getservbyname("http", "tcp");
	if (sp == 0)
		httpport = htons(HTTP_PORT);	/* good fallback */
	else
		httpport = sp->s_port;
	gateport = 0;
	cp = getenv("FTPSERVERPORT");
	if (cp != NULL) {
		port = strtol(cp, &ep, 10);
		if (port < 1 || port > 0xffff || *ep != '\0')
			warnx("bad FTPSERVERPORT port number: %s (ignored)",
			    cp);
		else
			gateport = htons(port);
	}
	if (gateport == 0) {
		sp = getservbyname("ftpgate", "tcp");
		if (sp == 0)
			gateport = htons(GATE_PORT);
		else
			gateport = sp->s_port;
	}
	doglob = 1;
	interactive = 1;
	autologin = 1;
	passivemode = 0;
	restricted_data_ports = 1;
	preserve = 1;
	verbose = 0;
	progress = 0;
	gatemode = 0;
#ifndef SMALL
	editing = 0;
	el = NULL;
	hist = NULL;
#endif
	mark = HASHBYTES;
	marg_sl = sl_init();
	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;

	cp = strrchr(argv[0], '/');
	cp = (cp == NULL) ? argv[0] : cp + 1;
	if (getenv("FTP_PASSIVE_MODE") || strcmp(cp, "pftp") == 0)
		passivemode = 1;
	else if (strcmp(cp, "gate-ftp") == 0)
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
	if (fromatty) {
		verbose = 1;		/* verbose if from a tty */
#ifndef SMALL
		if (! dumbterm)
			editing = 1;	/* editing mode on if tty is usable */
#endif
	}
	if (isatty(fileno(stdout)) && !dumbterm)
		progress = 1;		/* progress bar on if tty is usable */

	while ((ch = getopt(argc, argv, "adeginpP:tvVU")) != -1) {
		switch (ch) {
		case 'a':
			anonftp = 1;
			break;

		case 'd':
			options |= SO_DEBUG;
			debug++;
			break;

		case 'e':
#ifndef SMALL
			editing = 0;
#endif
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

		case 'p':
			passivemode = 1;
			break;

		case 'P':
			port = strtol(optarg, &ep, 10);
			if (port < 1 || port > 0xffff || *ep != '\0')
				warnx("bad port number: %s (ignored)", optarg);
			else
				ftpport = htons(port);
			break;

		case 't':
			trace = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'V':
			verbose = 0;
			break;

		case 'U':
		        restricted_data_ports = 0;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
	crflag = 1;	/* strip c.r. on ascii gets */
	sendport = -1;	/* not using ports */
	/*
	 * Set up the home directory in case we're globbing.
	 */
	cp = getlogin();
	if (cp != NULL) {
		pw = getpwnam(cp);
	}
	if (pw == NULL)
		pw = getpwuid(getuid());
	if (pw != NULL) {
		home = homedir;
		(void)strcpy(home, pw->pw_dir);
	}

	setttywidth(0);
	(void)signal(SIGWINCH, setttywidth);

#ifdef __GNUC__			/* XXX: to shut up gcc warnings */
	(void)&argc;
	(void)&argv;
#endif

	if (argc > 0) {
		if (strchr(argv[0], ':') != NULL) {
			anonftp = 1;	/* Handle "automatic" transfers. */
			rval = auto_fetch(argc, argv);
			if (rval >= 0)		/* -1 == connected and cd-ed */
				exit(rval);
		} else {
			char *xargv[5];

			if (setjmp(toplevel))
				exit(0);
			(void)signal(SIGINT, (sig_t)intr);
			(void)signal(SIGPIPE, (sig_t)lostpeer);
			xargv[0] = __progname;
			xargv[1] = argv[0];
			xargv[2] = argv[1];
			xargv[3] = argv[2];
			xargv[4] = NULL;
			setpeer(argc+1, xargv);
		}
	}
#ifndef SMALL
	controlediting();
#endif /* !SMALL */
	top = setjmp(toplevel) == 0;
	if (top) {
		(void)signal(SIGINT, (sig_t)intr);
		(void)signal(SIGPIPE, (sig_t)lostpeer);
	}
	for (;;) {
		cmdscanner(top);
		top = 1;
	}
}

void
intr()
{

	alarmtimer(0);
	longjmp(toplevel, 1);
}

void
lostpeer()
{

	alarmtimer(0);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), 1+1);
			(void)fclose(cout);
			cout = NULL;
		}
		if (data >= 0) {
			(void)shutdown(data, 1+1);
			(void)close(data);
			data = -1;
		}
		connected = 0;
	}
	pswitch(1);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), 1+1);
			(void)fclose(cout);
			cout = NULL;
		}
		connected = 0;
	}
	proxflag = 0;
	pswitch(0);
}

/*
 * Generate a prompt
 */
char *
prompt()
{
	return ("ftp> ");
}

/*
 * Command parser.
 */
void
cmdscanner(top)
	int top;
{
	struct cmd *c;
	int num;

	if (!top 
#ifndef SMALL
	    && !editing
#endif /* !SMALL */
	    )
		(void)putchar('\n');
	for (;;) {
#ifndef SMALL
		if (!editing) {
#endif /* !SMALL */
			if (fromatty) {
				fputs(prompt(), stdout);
				(void)fflush(stdout);
			}
			if (fgets(line, sizeof(line), stdin) == NULL)
				quit(0, 0);
			num = strlen(line);
			if (num == 0)
				break;
			if (line[--num] == '\n') {
				if (num == 0)
					break;
				line[num] = '\0';
			} else if (num == sizeof(line) - 2) {
				puts("sorry, input line too long.");
				while ((num = getchar()) != '\n' && num != EOF)
					/* void */;
				break;
			} /* else it was a line without a newline */
#ifndef SMALL
		} else {
			const char *buf;
			cursor_pos = NULL;

			if ((buf = el_gets(el, &num)) == NULL || num == 0)
				quit(0, 0);
			if (line[--num] == '\n') {
				if (num == 0)
					break;
			} else if (num >= sizeof(line)) {
				puts("sorry, input line too long.");
				break;
			}
			memcpy(line, buf, num);
			line[num] = '\0';
			history(hist, H_ENTER, buf);
		}
#endif /* !SMALL */

		makeargv();
		if (margc == 0)
			continue;
#if 0 && !defined(SMALL)	/* XXX: don't want el_parse */
		/*
		 * el_parse returns -1 to signal that it's not been handled
		 * internally.
		 */
		if (el_parse(el, margc, margv) != -1)
			continue;
#endif /* !SMALL */
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			puts("?Ambiguous command.");
			continue;
		}
		if (c == 0) {
			puts("?Invalid command.");
			continue;
		}
		if (c->c_conn && !connected) {
			puts("Not connected.");
			continue;
		}
		confirmrest = 0;
		(*c->c_handler)(margc, margv);
		if (bell && c->c_bell)
			(void)putchar('\007');
		if (c->c_handler != help)
			break;
	}
	(void)signal(SIGINT, (sig_t)intr);
	(void)signal(SIGPIPE, (sig_t)lostpeer);
}

struct cmd *
getcmd(name)
	const char *name;
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
makeargv()
{
	char *argp;

	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	marg_sl->sl_cur = 0;		/* reset to start of marg_sl */
	for (margc = 0; ; margc++) {
		argp = slurpstring();
		sl_add(marg_sl, argp);
		if (argp == NULL)
			break;
	}
#ifndef SMALL
	if (cursor_pos == line) {
		cursor_argc = 0;
		cursor_argo = 0;
	} else if (cursor_pos != NULL) {
		cursor_argc = margc;
		cursor_argo = strlen(margv[margc-1]);
	}
#endif /* !SMALL */
}

#ifdef SMALL
#define INC_CHKCURSOR(x)	(x)++
#else  /* !SMALL */
#define INC_CHKCURSOR(x)	{ (x)++ ; \
				if (x == cursor_pos) { \
					cursor_argc = margc; \
					cursor_argo = ap-argbase; \
					cursor_pos = NULL; \
				} }
						
#endif /* !SMALL */

/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
char *
slurpstring()
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
			altarg = (char *) 0;
			break;
		default:
			break;
	}
	return ((char *)0);
}

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help(argc, argv)
	int argc;
	char *argv[];
{
	struct cmd *c;

	if (argc == 1) {
		StringList *buf;

		buf = sl_init();
		printf("%sommands may be abbreviated.  Commands are:\n\n",
		    proxy ? "Proxy c" : "C");
		for (c = cmdtab; c < &cmdtab[NCMDS]; c++)
			if (c->c_name && (!proxy || c->c_proxy))
				sl_add(buf, c->c_name);
		list_vertical(buf);
		sl_free(buf, 0);
		return;
	}

#define HELPINDENT ((int) sizeof("disconnect"))

	while (--argc > 0) {
		char *arg;

		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%-*s\t%s\n", HELPINDENT,
				c->c_name, c->c_help);
	}
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-adeginptvV] [host [port]]\n"
	    "       %s host:path[/]\n"
	    "       %s ftp://host[:port]/path[/]\n"
	    "       %s http://host[:port]/file\n",
	    __progname, __progname, __progname, __progname);
	exit(1);
}
