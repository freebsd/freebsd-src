/*	$Id: cmds.c,v 1.8 1997/06/27 09:30:01 ache Exp $ */
/*	$NetBSD: cmds.c,v 1.24 1997/05/17 19:44:36 pk Exp $	*/

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

#ifndef lint
#if 0
static char sccsid[] = "@(#)cmds.c	8.6 (Berkeley) 10/9/94";
#else
static char rcsid[] = "$Id: cmds.c,v 1.8 1997/06/27 09:30:01 ache Exp $";
#endif
#endif /* not lint */

/*
 * FTP User Program -- Command Routines.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <glob.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp_var.h"
#include "pathnames.h"

jmp_buf	jabort;
char   *mname;
char   *home = "/";

struct	types {
	char	*t_name;
	char	*t_mode;
	int	t_type;
	char	*t_arg;
} types[] = {
	{ "ascii",	"A",	TYPE_A,	0 },
	{ "binary",	"I",	TYPE_I,	0 },
	{ "image",	"I",	TYPE_I,	0 },
	{ "ebcdic",	"E",	TYPE_E,	0 },
	{ "tenex",	"L",	TYPE_L,	bytename },
	{ NULL }
};

/*
 * Set transfer type.
 */
void
settype(argc, argv)
	int argc;
	char *argv[];
{
	struct types *p;
	int comret;

	if (argc > 2) {
		char *sep;

		printf("usage: %s [", argv[0]);
		sep = " ";
		for (p = types; p->t_name; p++) {
			printf("%s%s", sep, p->t_name);
			sep = " | ";
		}
		puts(" ]");
		code = -1;
		return;
	}
	if (argc < 2) {
		printf("Using %s mode to transfer files.\n", typename);
		code = 0;
		return;
	}
	for (p = types; p->t_name; p++)
		if (strcmp(argv[1], p->t_name) == 0)
			break;
	if (p->t_name == 0) {
		printf("%s: unknown mode.\n", argv[1]);
		code = -1;
		return;
	}
	if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
		comret = command("TYPE %s %s", p->t_mode, p->t_arg);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void)strcpy(typename, p->t_name);
		curtype = type = p->t_type;
	}
}

/*
 * Internal form of settype; changes current type in use with server
 * without changing our notion of the type for data transfers.
 * Used to change to and from ascii for listings.
 */
void
changetype(newtype, show)
	int newtype, show;
{
	struct types *p;
	int comret, oldverbose = verbose;

	if (newtype == 0)
		newtype = TYPE_I;
	if (newtype == curtype)
		return;
	if (debug == 0 && show == 0)
		verbose = 0;
	for (p = types; p->t_name; p++)
		if (newtype == p->t_type)
			break;
	if (p->t_name == 0) {
		warnx("internal error: unknown type %d.", newtype);
		return;
	}
	if (newtype == TYPE_L && bytename[0] != '\0')
		comret = command("TYPE %s %s", p->t_mode, bytename);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE)
		curtype = newtype;
	verbose = oldverbose;
}

char *stype[] = {
	"type",
	"",
	0
};

/*
 * Set binary transfer type.
 */
/*VARARGS*/
void
setbinary(argc, argv)
	int argc;
	char *argv[];
{

	stype[1] = "binary";
	settype(2, stype);
}

/*
 * Set ascii transfer type.
 */
/*VARARGS*/
void
setascii(argc, argv)
	int argc;
	char *argv[];
{

	stype[1] = "ascii";
	settype(2, stype);
}

/*
 * Set tenex transfer type.
 */
/*VARARGS*/
void
settenex(argc, argv)
	int argc;
	char *argv[];
{

	stype[1] = "tenex";
	settype(2, stype);
}

/*
 * Set file transfer mode.
 */
/*ARGSUSED*/
void
setftmode(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s mode, sorry.\n", modename);
	code = -1;
}

/*
 * Set file transfer format.
 */
/*ARGSUSED*/
void
setform(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s format, sorry.\n", formname);
	code = -1;
}

/*
 * Set file transfer structure.
 */
/*ARGSUSED*/
void
setstruct(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s structure, sorry.\n", structname);
	code = -1;
}

/*
 * Send a single file.
 */
void
put(argc, argv)
	int argc;
	char *argv[];
{
	char *cmd;
	int loc = 0;
	char *oldargv1, *oldargv2;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2 && !another(&argc, &argv, "local-file"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "remote-file")) || argc > 3) {
usage:
		printf("usage: %s local-file [ remote-file ]\n", argv[0]);
		code = -1;
		return;
	}
	oldargv1 = argv[1];
	oldargv2 = argv[2];
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	/*
	 * If "globulize" modifies argv[1], and argv[2] is a copy of
	 * the old argv[1], make it a copy of the new argv[1].
	 */
	if (argv[1] != oldargv1 && argv[2] == oldargv1) {
		argv[2] = argv[1];
	}
	cmd = (argv[0][0] == 'a') ? "APPE" : ((sunique) ? "STOU" : "STOR");
	if (loc && ntflag) {
		argv[2] = dotrans(argv[2]);
	}
	if (loc && mapflag) {
		argv[2] = domap(argv[2]);
	}
	sendrequest(cmd, argv[1], argv[2],
	    argv[1] != oldargv1 || argv[2] != oldargv2);
}

/*
 * Send multiple files.
 */
void
mput(argc, argv)
	int argc;
	char *argv[];
{
	int i;
	sig_t oldintr;
	int ointer;
	char *tp;

	if (argc < 2 && !another(&argc, &argv, "local-files")) {
		printf("usage: %s local-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	if (proxy) {
		char *cp, *tp2, tmpbuf[MAXPATHLEN];

		while ((cp = remglob(argv, 0, NULL)) != NULL) {
			if (*cp == '\0') {
				mflag = 0;
				continue;
			}
			if (mflag && confirm(argv[0], cp)) {
				tp = cp;
				if (mcase) {
					while (*tp && !islower((unsigned char)*tp)) {
						tp++;
					}
					if (!*tp) {
						tp = cp;
						tp2 = tmpbuf;
						while ((*tp2 = *tp) != '\0') {
						     if (isupper((unsigned char)*tp2))
							*tp2 = tolower((unsigned char)*tp2);
						     tp++;
						     tp2++;
						}
					}
					tp = tmpbuf;
				}
				if (ntflag) {
					tp = dotrans(tp);
				}
				if (mapflag) {
					tp = domap(tp);
				}
				sendrequest((sunique) ? "STOU" : "STOR",
				    cp, tp, cp != tp || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		(void)signal(SIGINT, oldintr);
		mflag = 0;
		return;
	}
	for (i = 1; i < argc; i++) {
		char **cpp;
		glob_t gl;
		int flags;

		if (!doglob) {
			if (mflag && confirm(argv[0], argv[i])) {
				tp = (ntflag) ? dotrans(argv[i]) : argv[i];
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    argv[i], tp, tp != argv[i] || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
			continue;
		}

		memset(&gl, 0, sizeof(gl));
		flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;
		if (glob(argv[i], flags, NULL, &gl) || gl.gl_pathc == 0) {
			warnx("%s: not found", argv[i]);
			globfree(&gl);
			continue;
		}
		for (cpp = gl.gl_pathv; cpp && *cpp != NULL; cpp++) {
			if (mflag && confirm(argv[0], *cpp)) {
				tp = (ntflag) ? dotrans(*cpp) : *cpp;
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    *cpp, tp, *cpp != tp || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with", "mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		globfree(&gl);
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
}

void
reget(argc, argv)
	int argc;
	char *argv[];
{

	(void)getit(argc, argv, 1, "r+w");
}

void
get(argc, argv)
	int argc;
	char *argv[];
{

	(void)getit(argc, argv, 0, restart_point ? "r+w" : "w" );
}

/*
 * Receive one file.
 */
int
getit(argc, argv, restartit, mode)
	int argc;
	char *argv[];
	int restartit;
	const char *mode;
{
	int loc = 0;
	char *oldargv1, *oldargv2;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2 && !another(&argc, &argv, "remote-file"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "local-file")) || argc > 3) {
usage:
		printf("usage: %s remote-file [ local-file ]\n", argv[0]);
		code = -1;
		return (0);
	}
	oldargv1 = argv[1];
	oldargv2 = argv[2];
	if (!globulize(&argv[2])) {
		code = -1;
		return (0);
	}
	if (loc && mcase) {
		char *tp = argv[1], *tp2, tmpbuf[MAXPATHLEN];

		while (*tp && !islower((unsigned char)*tp)) {
			tp++;
		}
		if (!*tp) {
			tp = argv[2];
			tp2 = tmpbuf;
			while ((*tp2 = *tp) != '\0') {
				if (isupper((unsigned char)*tp2))
					*tp2 = tolower((unsigned char)*tp2);
				tp++;
				tp2++;
			}
			argv[2] = tmpbuf;
		}
	}
	if (loc && ntflag)
		argv[2] = dotrans(argv[2]);
	if (loc && mapflag)
		argv[2] = domap(argv[2]);
	if (restartit) {
		struct stat stbuf;
		int ret;

		ret = stat(argv[2], &stbuf);
		if (restartit == 1) {
			if (ret < 0) {
				warn("local: %s", argv[2]);
				return (0);
			}
			restart_point = stbuf.st_size;
		} else {
			if (ret == 0) {
				time_t mtime;

				mtime = remotemodtime(argv[1], 0);
				if (mtime == -1)
					return (0);
				if (stbuf.st_mtime >= mtime)
					return (1);
			}
		}
	}

	recvrequest("RETR", argv[2], argv[1], mode,
	    argv[1] != oldargv1 || argv[2] != oldargv2);
	restart_point = 0;
	return (0);
}

/* ARGSUSED */
void
mabort(signo)
	int signo;
{
	int ointer, oconf;

	alarmtimer(0);
	putchar('\n');
	(void)fflush(stdout);
	if (mflag && fromatty) {
		ointer = interactive;
		oconf = confirmrest;
		interactive = 1;
		confirmrest = 0;
		if (confirm("Continue with", mname)) {
			interactive = ointer;
			confirmrest = oconf;
			longjmp(jabort, 0);
		}
		interactive = ointer;
		confirmrest = oconf;
	}
	mflag = 0;
	longjmp(jabort, 0);
}

/*
 * Get multiple files.
 */
void
mget(argc, argv)
	int argc;
	char *argv[];
{
	sig_t oldintr;
	int ch, ointer;
	char *cp, *tp, *tp2, tmpbuf[MAXPATHLEN];

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	while ((cp = remglob(argv, proxy, NULL)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			tp = cp;
			if (mcase) {
				for (tp2 = tmpbuf; (ch = (unsigned char)*tp++) != 0; )
					*tp2++ = isupper(ch) ? tolower(ch) : ch;
				*tp2 = '\0';
				tp = tmpbuf;
			}
			if (ntflag) {
				tp = dotrans(tp);
			}
			if (mapflag) {
				tp = domap(tp);
			}
			recvrequest("RETR", tp, cp, "w",
			    tp != cp || !interactive);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with", "mget")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
}

char *
onoff(bool)
	int bool;
{

	return (bool ? "on" : "off");
}

/*
 * Show status.
 */
/*ARGSUSED*/
void
status(argc, argv)
	int argc;
	char *argv[];
{
	int i;

	if (connected)
		printf("Connected %sto %s.\n",
		    connected == -1 ? "and logged in" : "", hostname);
	else
		puts("Not connected.");
	if (!proxy) {
		pswitch(1);
		if (connected) {
			printf("Connected for proxy commands to %s.\n",
			    hostname);
		}
		else {
			puts("No proxy connection.");
		}
		pswitch(0);
	}
	printf("Passive mode: %s.\n", onoff(passivemode));
	printf("Mode: %s; Type: %s; Form: %s; Structure: %s.\n",
		modename, typename, formname, structname);
	printf("Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s.\n",
		onoff(verbose), onoff(bell), onoff(interactive),
		onoff(doglob));
	printf("Store unique: %s; Receive unique: %s.\n", onoff(sunique),
		onoff(runique));
	printf("Preserve modification times: %s.\n", onoff(preserve));
	printf("Case: %s; CR stripping: %s.\n", onoff(mcase), onoff(crflag));
	if (ntflag) {
		printf("Ntrans: (in) %s (out) %s\n", ntin, ntout);
	}
	else {
		puts("Ntrans: off.");
	}
	if (mapflag) {
		printf("Nmap: (in) %s (out) %s\n", mapin, mapout);
	}
	else {
		puts("Nmap: off.");
	}
	printf("Hash mark printing: %s; Mark count: %d; Progress bar: %s.\n",
	    onoff(hash), mark, onoff(progress));
	printf("Use of PORT cmds: %s.\n", onoff(sendport));
#ifndef SMALL
	printf("Command line editing: %s.\n", onoff(editing));
#endif /* !SMALL */
	if (macnum > 0) {
		puts("Macros:");
		for (i=0; i<macnum; i++) {
			printf("\t%s\n", macros[i].mac_name);
		}
	}
	code = 0;
}

/*
 * Toggle a variable
 */
int
togglevar(argc, argv, var, mesg)
	int   argc;
	char *argv[];
	int  *var;
	const char *mesg;
{
	if (argc < 2) {
		*var = !*var;
	} else if (argc == 2 && strcasecmp(argv[1], "on") == 0) {
		*var = 1;
	} else if (argc == 2 && strcasecmp(argv[1], "off") == 0) {
		*var = 0;
	} else {
		printf("usage: %s [ on | off ]\n", argv[0]);
		return (-1);
	}
	if (mesg)
		printf("%s %s.\n", mesg, onoff(*var));
	return (*var);
}

/*
 * Set beep on cmd completed mode.
 */
/*VARARGS*/
void
setbell(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &bell, "Bell mode");
}

#ifndef SMALL
/*
 * Set command line editing
 */
/*VARARGS*/
void
setedit(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &editing, "Editing mode");
	controlediting();
}
#endif /* !SMALL */

/*
 * Turn on packet tracing.
 */
/*VARARGS*/
void
settrace(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &trace, "Packet tracing");
}

/*
 * Toggle hash mark printing during transfers, or set hash mark bytecount.
 */
/*VARARGS*/
void
sethash(argc, argv)
	int argc;
	char *argv[];
{
	if (argc == 1)
		hash = !hash;
	else if (argc != 2) {
		printf("usage: %s [ on | off | bytecount ]\n", argv[0]);
		code = -1;
		return;
	} else if (strcasecmp(argv[1], "on") == 0)
		hash = 1;
	else if (strcasecmp(argv[1], "off") == 0)
		hash = 0;
	else {
		int nmark = atol(argv[1]);
		if (nmark < 1) {
			printf("%s: bad bytecount value.\n", argv[1]);
			code = -1;
			return;
		}
		mark = nmark;
		hash = 1;
	}
	printf("Hash mark printing %s", onoff(hash));
	if (hash)
		printf(" (%d bytes/hash mark)", mark);
	puts(".");
	code = hash;
}

/*
 * Turn on printing of server echo's.
 */
/*VARARGS*/
void
setverbose(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &verbose, "Verbose mode");
}

/*
 * Toggle PORT cmd use before each data connection.
 */
/*VARARGS*/
void
setport(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &sendport, "Use of PORT cmds");
}

/*
 * Toggle transfer progress bar.
 */
/*VARARGS*/
void
setprogress(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &progress, "Progress bar");
}

/*
 * Turn on interactive prompting
 * during mget, mput, and mdelete.
 */
/*VARARGS*/
void
setprompt(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &interactive, "Interactive mode");
}

/*
 * Toggle metacharacter interpretation
 * on local file names.
 */
/*VARARGS*/
void
setglob(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &doglob, "Globbing");
}

/*
 * Toggle preserving modification times on retreived files.
 */
/*VARARGS*/
void
setpreserve(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &preserve, "Preserve modification times");
}

/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
/*VARARGS*/
void
setdebug(argc, argv)
	int argc;
	char *argv[];
{
	int val;

	if (argc > 2) {
		printf("usage: %s [ on | off | debuglevel ]\n", argv[0]);
		code = -1;
		return;
	} else if (argc == 2) {
		if (strcasecmp(argv[1], "on") == 0)
			debug = 1;
		else if (strcasecmp(argv[1], "off") == 0)
			debug = 0;
		else {
			val = atoi(argv[1]);
			if (val < 0) {
				printf("%s: bad debugging value.\n", argv[1]);
				code = -1;
				return;
			}
			debug = val;
		}
	} else
		debug = !debug;
	if (debug)
		options |= SO_DEBUG;
	else
		options &= ~SO_DEBUG;
	printf("Debugging %s (debug=%d).\n", onoff(debug), debug);
	code = debug > 0;
}

/*
 * Set current working directory
 * on remote machine.
 */
void
cd(argc, argv)
	int argc;
	char *argv[];
{
	int r;

	if ((argc < 2 && !another(&argc, &argv, "remote-directory")) ||
	    argc > 2) {
		printf("usage: %s remote-directory\n", argv[0]);
		code = -1;
		return;
	}
	r = command("CWD %s", argv[1]);
	if (r == ERROR && code == 500) {
		if (verbose)
			puts("CWD command not recognized, trying XCWD.");
		r = command("XCWD %s", argv[1]);
	}
	if (r == COMPLETE)
		dirchange = 1;
}

/*
 * Set current working directory
 * on local machine.
 */
void
lcd(argc, argv)
	int argc;
	char *argv[];
{
	char buf[MAXPATHLEN];

	if (argc < 2)
		argc++, argv[1] = home;
	if (argc != 2) {
		printf("usage: %s local-directory\n", argv[0]);
		code = -1;
		return;
	}
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	if (chdir(argv[1]) < 0) {
		warn("local: %s", argv[1]);
		code = -1;
		return;
	}
	if (getcwd(buf, sizeof(buf)) != NULL)
		printf("Local directory now %s\n", buf);
	else
		warn("getcwd: %s", argv[1]);
	code = 0;
}

/*
 * Delete a single file.
 */
void
delete(argc, argv)
	int argc;
	char *argv[];
{

	if ((argc < 2 && !another(&argc, &argv, "remote-file")) || argc > 2) {
		printf("usage: %s remote-file\n", argv[0]);
		code = -1;
		return;
	}
	(void)command("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
void
mdelete(argc, argv)
	int argc;
	char *argv[];
{
	sig_t oldintr;
	int ointer;
	char *cp;

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	while ((cp = remglob(argv, 0, NULL)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			(void)command("DELE %s", cp);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with", "mdelete")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Rename a remote file.
 */
void
renamefile(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "from-name"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "to-name")) || argc > 3) {
usage:
		printf("usage: %s from-name to-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RNFR %s", argv[1]) == CONTINUE)
		(void)command("RNTO %s", argv[2]);
}

/*
 * Get a directory listing
 * of remote files.
 */
void
ls(argc, argv)
	int argc;
	char *argv[];
{
	const char *cmd;

	if (argc < 2)
		argc++, argv[1] = NULL;
	if (argc < 3)
		argc++, argv[2] = "-";
	if (argc > 3) {
		printf("usage: %s remote-directory local-file\n", argv[0]);
		code = -1;
		return;
	}
	cmd = strcmp(argv[0], "dir") == 0 ? "LIST" : "NLST";
	if (strcmp(argv[2], "-") && !globulize(&argv[2])) {
		code = -1;
		return;
	}
	if (strcmp(argv[2], "-") && *argv[2] != '|')
		if (!globulize(&argv[2]) || !confirm("output to local-file:",
		    argv[2])) {
			code = -1;
			return;
	}
	recvrequest(cmd, argv[2], argv[1], "w", 0);

	/* flush results in case commands are coming from a pipe */
	fflush(stdout);
}

/*
 * Get a directory listing
 * of multiple remote files.
 */
void
mls(argc, argv)
	int argc;
	char *argv[];
{
	sig_t oldintr;
	int ointer, i;
	const char *cmd;
	char mode[1], *dest;

	if (argc < 2 && !another(&argc, &argv, "remote-files"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "local-file")) {
usage:
		printf("usage: %s remote-files local-file\n", argv[0]);
		code = -1;
		return;
	}
	dest = argv[argc - 1];
	argv[argc - 1] = NULL;
	if (strcmp(dest, "-") && *dest != '|')
		if (!globulize(&dest) ||
		    !confirm("output to local-file:", dest)) {
			code = -1;
			return;
	}
	cmd = strcmp(argv[0], "mls") == 0 ? "NLST" : "LIST";
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	for (i = 1; mflag && i < argc-1; ++i) {
		*mode = (i == 1) ? 'w' : 'a';
		recvrequest(cmd, dest, argv[i], mode, 0);
		if (!mflag && fromatty) {
			ointer = interactive;
			interactive = 1;
			if (confirm("Continue with", argv[0])) {
				mflag ++;
			}
			interactive = ointer;
		}
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Do a shell escape
 */
/*ARGSUSED*/
void
shell(argc, argv)
	int argc;
	char *argv[];
{
	pid_t pid;
	sig_t old1, old2;
	char shellnam[MAXPATHLEN], *shell, *namep;
	union wait status;

	old1 = signal (SIGINT, SIG_IGN);
	old2 = signal (SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		for (pid = 3; pid < 20; pid++)
			(void)close(pid);
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);
		shell = getenv("SHELL");
		if (shell == NULL)
			shell = _PATH_BSHELL;
		namep = strrchr(shell, '/');
		if (namep == NULL)
			namep = shell;
		shellnam[0] = '-';
		(void)strncpy(shellnam + 1, ++namep, sizeof(shellnam) - 2);
		shellnam[sizeof(shellnam) - 1] = '\0';
		if (strcmp(namep, "sh") != 0)
			shellnam[0] = '+';
		if (debug) {
			puts(shell);
			(void)fflush(stdout);
		}
		if (argc > 1) {
			execl(shell, shellnam, "-c", altarg, (char *)0);
		}
		else {
			execl(shell, shellnam, (char *)0);
		}
		warn("%s", shell);
		code = -1;
		exit(1);
	}
	if (pid > 0)
		while (wait((int *)&status) != pid)
			;
	(void)signal(SIGINT, old1);
	(void)signal(SIGQUIT, old2);
	if (pid == -1) {
		warn("Try again later");
		code = -1;
	}
	else {
		code = 0;
	}
}

/*
 * Send new user information (re-login)
 */
void
user(argc, argv)
	int argc;
	char *argv[];
{
	char acct[80];
	int n, aflag = 0;

	if (argc < 2)
		(void)another(&argc, &argv, "username");
	if (argc < 2 || argc > 4) {
		printf("usage: %s username [password] [account]\n", argv[0]);
		code = -1;
		return;
	}
	n = command("USER %s", argv[1]);
	if (n == CONTINUE) {
		if (argc < 3 )
			argv[2] = getpass("Password: "), argc++;
		n = command("PASS %s", argv[2]);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			(void)fputs("Account: ", stdout);
			(void)fflush(stdout);
			(void)fgets(acct, sizeof(acct) - 1, stdin);
			acct[strlen(acct) - 1] = '\0';
			argv[3] = acct; argc++;
		}
		n = command("ACCT %s", argv[3]);
		aflag++;
	}
	if (n != COMPLETE) {
		puts("Login failed.");
		return;
	}
	if (!aflag && argc == 4) {
		(void)command("ACCT %s", argv[3]);
	}
	connected = -1;
}

/*
 * Print working directory on remote machine.
 */
/*VARARGS*/
void
pwd(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	/*
	 * If we aren't verbose, this doesn't do anything!
	 */
	verbose = 1;
	if (command("PWD") == ERROR && code == 500) {
		puts("PWD command not recognized, trying XPWD.");
		(void)command("XPWD");
	}
	verbose = oldverbose;
}

/*
 * Print working directory on local machine.
 */
void
lpwd(argc, argv)
	int argc;
	char *argv[];
{
	char buf[MAXPATHLEN];

	if (getcwd(buf, sizeof(buf)) != NULL)
		printf("Local directory %s\n", buf);
	else
		warn("getcwd");
	code = 0;
}

/*
 * Make a directory.
 */
void
makedir(argc, argv)
	int argc;
	char *argv[];
{

	if ((argc < 2 && !another(&argc, &argv, "directory-name")) ||
	    argc > 2) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("MKD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			puts("MKD command not recognized, trying XMKD.");
		(void)command("XMKD %s", argv[1]);
	}
}

/*
 * Remove a directory.
 */
void
removedir(argc, argv)
	int argc;
	char *argv[];
{

	if ((argc < 2 && !another(&argc, &argv, "directory-name")) ||
	    argc > 2) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RMD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			puts("RMD command not recognized, trying XRMD.");
		(void)command("XRMD %s", argv[1]);
	}
}

/*
 * Send a line, verbatim, to the remote machine.
 */
void
quote(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "command line to send")) {
		printf("usage: %s line-to-send\n", argv[0]);
		code = -1;
		return;
	}
	quote1("", argc, argv);
}

/*
 * Send a SITE command to the remote machine.  The line
 * is sent verbatim to the remote machine, except that the
 * word "SITE" is added at the front.
 */
void
site(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "arguments to SITE command")) {
		printf("usage: %s line-to-send\n", argv[0]);
		code = -1;
		return;
	}
	quote1("SITE ", argc, argv);
}

/*
 * Turn argv[1..argc) into a space-separated string, then prepend initial text.
 * Send the result as a one-line command and get response.
 */
void
quote1(initial, argc, argv)
	const char *initial;
	int argc;
	char *argv[];
{
	int i, len;
	char buf[BUFSIZ];		/* must be >= sizeof(line) */

	(void)strncpy(buf, initial, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	if (argc > 1) {
		len = strlen(buf);
		len += strlen(strncpy(&buf[len], argv[1],
		    sizeof(buf) - len - 1));
		for (i = 2; i < argc && len < sizeof(buf); i++) {
			buf[len++] = ' ';
			len += strlen(strncpy(&buf[len], argv[i],
			    sizeof(buf) - len) - 1);
		}
	}
	if (command(buf) == PRELIM) {
		while (getreply(0) == PRELIM)
			continue;
	}
}

void
do_chmod(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "mode"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "file-name")) || argc > 3) {
usage:
		printf("usage: %s mode file-name\n", argv[0]);
		code = -1;
		return;
	}
	(void)command("SITE CHMOD %s %s", argv[1], argv[2]);
}

void
do_umask(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "SITE UMASK" : "SITE UMASK %s", argv[1]);
	verbose = oldverbose;
}

void
idle(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "SITE IDLE" : "SITE IDLE %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Ask the other side for help.
 */
void
rmthelp(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "HELP" : "HELP %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
/*VARARGS*/
void
quit(argc, argv)
	int argc;
	char *argv[];
{

	if (connected)
		disconnect(0, 0);
	pswitch(1);
	if (connected) {
		disconnect(0, 0);
	}
	exit(0);
}

/*
 * Terminate session, but don't exit.
 */
void
disconnect(argc, argv)
	int argc;
	char *argv[];
{

	if (!connected)
		return;
	(void)command("QUIT");
	if (cout) {
		(void)fclose(cout);
	}
	cout = NULL;
	connected = 0;
	data = -1;
	if (!proxy) {
		macnum = 0;
	}
}

void
account(argc, argv)
	int argc;
	char *argv[];
{
	char *ap;

	if (argc > 2) {
		printf("usage: %s [password]\n", argv[0]);
		code = -1;
		return;
	}
	else if (argc == 2)
		ap = argv[1];
	else
		ap = getpass("Account:");
	(void)command("ACCT %s", ap);
}

jmp_buf abortprox;

void
proxabort(notused)
	int notused;
{

	alarmtimer(0);
	if (!proxy) {
		pswitch(1);
	}
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	longjmp(abortprox, 1);
}

void
doproxy(argc, argv)
	int argc;
	char *argv[];
{
	struct cmd *c;
	int cmdpos;
	sig_t oldintr;

	if (argc < 2 && !another(&argc, &argv, "command")) {
		printf("usage: %s command\n", argv[0]);
		code = -1;
		return;
	}
	c = getcmd(argv[1]);
	if (c == (struct cmd *) -1) {
		puts("?Ambiguous command.");
		(void)fflush(stdout);
		code = -1;
		return;
	}
	if (c == 0) {
		puts("?Invalid command.");
		(void)fflush(stdout);
		code = -1;
		return;
	}
	if (!c->c_proxy) {
		puts("?Invalid proxy command.");
		(void)fflush(stdout);
		code = -1;
		return;
	}
	if (setjmp(abortprox)) {
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, proxabort);
	pswitch(1);
	if (c->c_conn && !connected) {
		puts("Not connected.");
		(void)fflush(stdout);
		pswitch(0);
		(void)signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	cmdpos = strcspn(line, " \t");
	if (cmdpos > 0)		/* remove leading "proxy " from input buffer */
		memmove(line, line + cmdpos + 1, strlen(line) - cmdpos + 1);
	(*c->c_handler)(argc-1, argv+1);
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	(void)signal(SIGINT, oldintr);
}

void
setcase(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &mcase, "Case mapping");
}

void
setcr(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &crflag, "Carriage Return stripping");
}

void
setntrans(argc, argv)
	int argc;
	char *argv[];
{
	if (argc == 1) {
		ntflag = 0;
		puts("Ntrans off.");
		code = ntflag;
		return;
	}
	ntflag++;
	code = ntflag;
	(void)strncpy(ntin, argv[1], sizeof(ntin) - 1);
	ntin[sizeof(ntin) - 1] = '\0';
	if (argc == 2) {
		ntout[0] = '\0';
		return;
	}
	(void)strncpy(ntout, argv[2], sizeof(ntout) - 1);
	ntout[sizeof(ntout) - 1] = '\0';
}

char *
dotrans(name)
	char *name;
{
	static char new[MAXPATHLEN];
	char *cp1, *cp2 = new;
	int i, ostop, found;

	for (ostop = 0; *(ntout + ostop) && ostop < 16; ostop++)
		continue;
	for (cp1 = name; *cp1; cp1++) {
		found = 0;
		for (i = 0; *(ntin + i) && i < 16; i++) {
			if (*cp1 == *(ntin + i)) {
				found++;
				if (i < ostop) {
					*cp2++ = *(ntout + i);
				}
				break;
			}
		}
		if (!found) {
			*cp2++ = *cp1;
		}
	}
	*cp2 = '\0';
	return (new);
}

void
setnmap(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;

	if (argc == 1) {
		mapflag = 0;
		puts("Nmap off.");
		code = mapflag;
		return;
	}
	if ((argc < 3 && !another(&argc, &argv, "mapout")) || argc > 3) {
		printf("usage: %s [mapin mapout]\n", argv[0]);
		code = -1;
		return;
	}
	mapflag = 1;
	code = 1;
	cp = strchr(altarg, ' ');
	if (proxy) {
		while(*++cp == ' ')
			continue;
		altarg = cp;
		cp = strchr(altarg, ' ');
	}
	*cp = '\0';
	(void)strncpy(mapin, altarg, MAXPATHLEN - 1);
	while (*++cp == ' ')
		continue;
	(void)strncpy(mapout, cp, MAXPATHLEN - 1);
}

char *
domap(name)
	char *name;
{
	static char new[MAXPATHLEN];
	char *cp1 = name, *cp2 = mapin;
	char *tp[9], *te[9];
	int i, toks[9], toknum = 0, match = 1;

	for (i=0; i < 9; ++i) {
		toks[i] = 0;
	}
	while (match && *cp1 && *cp2) {
		switch (*cp2) {
			case '\\':
				if (*++cp2 != *cp1) {
					match = 0;
				}
				break;
			case '$':
				if (*(cp2+1) >= '1' && (*cp2+1) <= '9') {
					if (*cp1 != *(++cp2+1)) {
						toks[toknum = *cp2 - '1']++;
						tp[toknum] = cp1;
						while (*++cp1 && *(cp2+1)
							!= *cp1);
						te[toknum] = cp1;
					}
					cp2++;
					break;
				}
				/* FALLTHROUGH */
			default:
				if (*cp2 != *cp1) {
					match = 0;
				}
				break;
		}
		if (match && *cp1) {
			cp1++;
		}
		if (match && *cp2) {
			cp2++;
		}
	}
	if (!match && *cp1) /* last token mismatch */
	{
		toks[toknum] = 0;
	}
	cp1 = new;
	*cp1 = '\0';
	cp2 = mapout;
	while (*cp2) {
		match = 0;
		switch (*cp2) {
			case '\\':
				if (*(cp2 + 1)) {
					*cp1++ = *++cp2;
				}
				break;
			case '[':
LOOP:
				if (*++cp2 == '$' && isdigit((unsigned char)*(cp2+1))) {
					if (*++cp2 == '0') {
						char *cp3 = name;

						while (*cp3) {
							*cp1++ = *cp3++;
						}
						match = 1;
					}
					else if (toks[toknum = *cp2 - '1']) {
						char *cp3 = tp[toknum];

						while (cp3 != te[toknum]) {
							*cp1++ = *cp3++;
						}
						match = 1;
					}
				}
				else {
					while (*cp2 && *cp2 != ',' &&
					    *cp2 != ']') {
						if (*cp2 == '\\') {
							cp2++;
						}
						else if (*cp2 == '$' &&
							isdigit((unsigned char)*(cp2+1))) {
							if (*++cp2 == '0') {
							   char *cp3 = name;

							   while (*cp3) {
								*cp1++ = *cp3++;
							   }
							}
							else if (toks[toknum =
							    *cp2 - '1']) {
							   char *cp3=tp[toknum];

							   while (cp3 !=
								  te[toknum]) {
								*cp1++ = *cp3++;
							   }
							}
						}
						else if (*cp2) {
							*cp1++ = *cp2++;
						}
					}
					if (!*cp2) {
						puts(
"nmap: unbalanced brackets.");
						return (name);
					}
					match = 1;
					cp2--;
				}
				if (match) {
					while (*++cp2 && *cp2 != ']') {
					      if (*cp2 == '\\' && *(cp2 + 1)) {
							cp2++;
					      }
					}
					if (!*cp2) {
						puts(
"nmap: unbalanced brackets.");
						return (name);
					}
					break;
				}
				switch (*++cp2) {
					case ',':
						goto LOOP;
					case ']':
						break;
					default:
						cp2--;
						goto LOOP;
				}
				break;
			case '$':
				if (isdigit((unsigned char)*(cp2 + 1))) {
					if (*++cp2 == '0') {
						char *cp3 = name;

						while (*cp3) {
							*cp1++ = *cp3++;
						}
					}
					else if (toks[toknum = *cp2 - '1']) {
						char *cp3 = tp[toknum];

						while (cp3 != te[toknum]) {
							*cp1++ = *cp3++;
						}
					}
					break;
				}
				/* intentional drop through */
			default:
				*cp1++ = *cp2;
				break;
		}
		cp2++;
	}
	*cp1 = '\0';
	if (!*new) {
		return (name);
	}
	return (new);
}

void
setpassive(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &passivemode,
	    verbose ? "Passive mode" : NULL);
}

void
setsunique(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &sunique, "Store unique");
}

void
setrunique(argc, argv)
	int argc;
	char *argv[];
{

	code = togglevar(argc, argv, &runique, "Receive unique");
}

/* change directory to parent directory */
void
cdup(argc, argv)
	int argc;
	char *argv[];
{
	int r;

	r = command("CDUP");
	if (r == ERROR && code == 500) {
		if (verbose)
			puts("CDUP command not recognized, trying XCUP.");
		r = command("XCUP");
	}
	if (r == COMPLETE)
		dirchange = 1;
}

/* restart transfer at specific point */
void
restart(argc, argv)
	int argc;
	char *argv[];
{

	if (argc != 2)
		puts("restart: offset not specified.");
	else {
		restart_point = atol(argv[1]);
		printf("Restarting at %qd. Execute get, put or append to"
			"initiate transfer\n", restart_point);
	}
}

/* show remote system type */
void
syst(argc, argv)
	int argc;
	char *argv[];
{

	(void)command("SYST");
}

void
macdef(argc, argv)
	int argc;
	char *argv[];
{
	char *tmp;
	int c;

	if (macnum == 16) {
		puts("Limit of 16 macros have already been defined.");
		code = -1;
		return;
	}
	if ((argc < 2 && !another(&argc, &argv, "macro name")) || argc > 2) {
		printf("usage: %s macro_name\n", argv[0]);
		code = -1;
		return;
	}
	if (interactive)
		puts(
"Enter macro line by line, terminating it with a null line.");
	(void)strncpy(macros[macnum].mac_name, argv[1],
	    sizeof(macros[macnum].mac_name) - 1);
	macros[macnum].mac_name[sizeof(macros[macnum].mac_name) - 1] = '\0';
	if (macnum == 0)
		macros[macnum].mac_start = macbuf;
	else
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	tmp = macros[macnum].mac_start;
	while (tmp != macbuf+4096) {
		if ((c = getchar()) == EOF) {
			puts("macdef: end of file encountered.");
			code = -1;
			return;
		}
		if ((*tmp = c) == '\n') {
			if (tmp == macros[macnum].mac_start) {
				macros[macnum++].mac_end = tmp;
				code = 0;
				return;
			}
			if (*(tmp-1) == '\0') {
				macros[macnum++].mac_end = tmp - 1;
				code = 0;
				return;
			}
			*tmp = '\0';
		}
		tmp++;
	}
	while (1) {
		while ((c = getchar()) != '\n' && c != EOF)
			/* LOOP */;
		if (c == EOF || getchar() == '\n') {
			puts("Macro not defined - 4K buffer exceeded.");
			code = -1;
			return;
		}
	}
}

/*
 * Restrict FTP data port range to a high group of "safe" ports
 */
void
setrestrict(argc, argv)
        int argc;
        char *argv[];
{
	code = togglevar(argc, argv, &restricted_data_ports,
			 verbose ? "Restricted data ports" : NULL);
}

/*
 * get size of file on remote machine
 */
void
sizecmd(argc, argv)
	int argc;
	char *argv[];
{
	off_t size;

	if ((argc < 2 && !another(&argc, &argv, "filename")) || argc > 2) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	size = remotesize(argv[1], 1);
	if (size != -1)
		printf("%s\t%qd\n", argv[1], size);
	code = size;
}

/*
 * get last modification time of file on remote machine
 */
void
modtime(argc, argv)
	int argc;
	char *argv[];
{
	time_t mtime;

	if ((argc < 2 && !another(&argc, &argv, "filename")) || argc > 2) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	mtime = remotemodtime(argv[1], 1);
	if (mtime != -1)
		printf("%s\t%s", argv[1], asctime(localtime(&mtime)));
	code = mtime;
}

/*
 * show status on remote machine
 */
void
rmtstatus(argc, argv)
	int argc;
	char *argv[];
{

	(void)command(argc > 1 ? "STAT %s" : "STAT" , argv[1]);
}

/*
 * get file if modtime is more recent than current file
 */
void
newer(argc, argv)
	int argc;
	char *argv[];
{

	if (getit(argc, argv, -1, "w"))
		printf("Local file \"%s\" is newer than remote file \"%s\".\n",
			argv[2], argv[1]);
}

/*
 * Display one file through $PAGER (defaults to "more").
 */
void
page(argc, argv)
	int argc;
	char *argv[];
{
	int orestart_point, ohash, overbose;
	char *p, *pager;

	if ((argc < 2 && !another(&argc, &argv, "filename")) || argc > 2) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	p = getenv("PAGER");
	if (p == NULL)
		p = PAGER;
	if ((pager = malloc(strlen(p) + 2)) == NULL)
		errx(1, "Can't allocate memory for $PAGER");
	(void)sprintf(pager, "|%s", p);

	orestart_point = restart_point;
	ohash = hash;
	overbose = verbose;
	restart_point = hash = verbose = 0;
	recvrequest("RETR", pager, argv[1], "r+w", 1);
	(void)free(pager);
	restart_point = orestart_point;
	hash = ohash;
	verbose = overbose;
}
