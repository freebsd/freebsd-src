/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
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
static char sccsid[] = "@(#)cmds.c	5.26 (Berkeley) 3/5/91";
#endif /* not lint */

/*
 * FTP User Program -- Command Routines.
 */
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <arpa/ftp.h>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <netinet/in.h>

#include "ftp_var.h"
#include "pathnames.h"

extern	char *globerr;
extern	char **ftpglob();
extern	char *home;
extern	char *remglob();
extern	char *getenv();
extern	char *index();
extern	char *rindex();
extern	char *strerror();
extern	int  errno;
extern off_t restart_point;
extern char reply_string[];

char *mname;
jmp_buf jabort;
char *dotrans(), *domap();

/*
 * `Another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
another(pargc, pargv, prompt)
	int *pargc;
	char ***pargv;
	char *prompt;
{
	int len = strlen(line), ret;
	extern sig_t intr();

	if (len >= sizeof(line) - 3) {
		printf("sorry, arguments too long\n");
		intr();
	}
	printf("(%s) ", prompt);
	line[len++] = ' ';
	if (fgets(&line[len], sizeof(line) - len, stdin) == NULL)
		intr();
	len += strlen(&line[len]);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	makeargv();
	ret = margc > *pargc;
	*pargc = margc;
	*pargv = margv;
	return (ret);
}

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	char *host, *hookup();
	short port;

	if (connected) {
		printf("Already connected to %s, use close first.\n",
			hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void) another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
		printf("usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	port = sp->s_port;
	if (argc > 2) {
		port = atoi(argv[2]);
		if (port <= 0) {
			printf("%s: bad port number-- %s\n", argv[1], argv[2]);
			printf ("usage: %s host-name [port]\n", argv[0]);
			code = -1;
			return;
		}
		port = htons(port);
	}
	host = hookup(argv[1], port);
	if (host) {
		int overbose;

		connected = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void) strcpy(typename, "ascii"), type = TYPE_A;
		curtype = TYPE_A;
		(void) strcpy(formname, "non-print"), form = FORM_N;
		(void) strcpy(modename, "stream"), mode = MODE_S;
		(void) strcpy(structname, "file"), stru = STRU_F;
		(void) strcpy(bytename, "8"), bytesize = 8;
		if (autologin)
			(void) login(argv[1]);

#if defined(unix) && NBBY == 8
/*
 * this ifdef is to keep someone form "porting" this to an incompatible
 * system and not checking this out. This way they have to think about it.
 */
		overbose = verbose;
		if (debug == 0)
			verbose = -1;
		if (command("SYST") == COMPLETE && overbose) {
			register char *cp, c;
			cp = index(reply_string+4, ' ');
			if (cp == NULL)
				cp = index(reply_string+4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			printf("Remote system type is %s.\n",
				reply_string+4);
			if (cp)
				*cp = c;
		}
		if (!strncmp(reply_string, "215 UNIX Type: L8", 17)) {
			if (proxy)
				unix_proxy = 1;
			else
				unix_server = 1;
			/*
			 * Set type to 0 (not specified by user),
			 * meaning binary by default, but don't bother
			 * telling server.  We can use binary
			 * for text files unless changed by the user.
			 */
			type = 0;
			(void) strcpy(typename, "binary");
			if (overbose)
			    printf("Using %s mode to transfer files.\n",
				typename);
		} else {
			if (proxy)
				unix_proxy = 0;
			else
				unix_server = 0;
			if (overbose && 
			    !strncmp(reply_string, "215 TOPS20", 10))
				printf(
"Remember to set tenex mode when transfering binary files from this machine.\n");
		}
		verbose = overbose;
#endif /* unix */
	}
}

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
	0
};

/*
 * Set transfer type.
 */
settype(argc, argv)
	int argc;
	char *argv[];
{
	register struct types *p;
	int comret;

	if (argc > 2) {
		char *sep;

		printf("usage: %s [", argv[0]);
		sep = " ";
		for (p = types; p->t_name; p++) {
			printf("%s%s", sep, p->t_name);
			sep = " | ";
		}
		printf(" ]\n");
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
		printf("%s: unknown mode\n", argv[1]);
		code = -1;
		return;
	}
	if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
		comret = command ("TYPE %s %s", p->t_mode, p->t_arg);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void) strcpy(typename, p->t_name);
		curtype = type = p->t_type;
	}
}

/*
 * Internal form of settype; changes current type in use with server
 * without changing our notion of the type for data transfers.
 * Used to change to and from ascii for listings.
 */
changetype(newtype, show)
	int newtype, show;
{
	register struct types *p;
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
		printf("ftp: internal error: unknown type %d\n", newtype);
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
setbinary()
{
	stype[1] = "binary";
	settype(2, stype);
}

/*
 * Set ascii transfer type.
 */
/*VARARGS*/
setascii()
{
	stype[1] = "ascii";
	settype(2, stype);
}

/*
 * Set tenex transfer type.
 */
/*VARARGS*/
settenex()
{
	stype[1] = "tenex";
	settype(2, stype);
}

/*
 * Set file transfer mode.
 */
/*ARGSUSED*/
setmode(argc, argv)
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
	if (argc < 3 && !another(&argc, &argv, "remote-file")) {
usage:
		printf("usage: %s local-file remote-file\n", argv[0]);
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
mput(argc, argv)
	int argc;
	char **argv;
{
	extern jmp_buf jabort;
	register int i;
	sig_t oldintr;
	int ointer;
	char *tp;
	void mabort();

	if (argc < 2 && !another(&argc, &argv, "local-files")) {
		printf("usage: %s local-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	if (proxy) {
		char *cp, *tp2, tmpbuf[MAXPATHLEN];

		while ((cp = remglob(argv,0)) != NULL) {
			if (*cp == 0) {
				mflag = 0;
				continue;
			}
			if (mflag && confirm(argv[0], cp)) {
				tp = cp;
				if (mcase) {
					while (*tp && !islower(*tp)) {
						tp++;
					}
					if (!*tp) {
						tp = cp;
						tp2 = tmpbuf;
						while ((*tp2 = *tp) != NULL) {
						     if (isupper(*tp2)) {
						        *tp2 = 'a' + *tp2 - 'A';
						     }
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
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		(void) signal(SIGINT, oldintr);
		mflag = 0;
		return;
	}
	for (i = 1; i < argc; i++) {
		register char **cpp, **gargs;

		if (!doglob) {
			if (mflag && confirm(argv[0], argv[i])) {
				tp = (ntflag) ? dotrans(argv[i]) : argv[i];
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    argv[i], tp, tp != argv[i] || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
			continue;
		}
		gargs = ftpglob(argv[i]);
		if (globerr != NULL) {
			printf("%s\n", globerr);
			if (gargs) {
				blkfree(gargs);
				free((char *)gargs);
			}
			continue;
		}
		for (cpp = gargs; cpp && *cpp != NULL; cpp++) {
			if (mflag && confirm(argv[0], *cpp)) {
				tp = (ntflag) ? dotrans(*cpp) : *cpp;
				tp = (mapflag) ? domap(tp) : tp;
				sendrequest((sunique) ? "STOU" : "STOR",
				    *cpp, tp, *cpp != tp || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}
			}
		}
		if (gargs != NULL) {
			blkfree(gargs);
			free((char *)gargs);
		}
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

reget(argc, argv)
	int argc;
	char *argv[];
{
	(void) getit(argc, argv, 1, "r+w");
}

get(argc, argv)
	int argc;
	char *argv[];
{
	(void) getit(argc, argv, 0, restart_point ? "r+w" : "w" );
}

/*
 * Receive one file.
 */
getit(argc, argv, restartit, mode)
	int argc;
	char *argv[];
	char *mode;
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
	if (argc < 3 && !another(&argc, &argv, "local-file")) {
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

		while (*tp && !islower(*tp)) {
			tp++;
		}
		if (!*tp) {
			tp = argv[2];
			tp2 = tmpbuf;
			while ((*tp2 = *tp) != NULL) {
				if (isupper(*tp2)) {
					*tp2 = 'a' + *tp2 - 'A';
				}
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
				fprintf(stderr, "local: %s: %s\n", argv[2],
					strerror(errno));
				return (0);
			}
			restart_point = stbuf.st_size;
		} else {
			if (ret == 0) {
				int overbose;

				overbose = verbose;
				if (debug == 0)
					verbose = -1;
				if (command("MDTM %s", argv[1]) == COMPLETE) {
					int yy, mo, day, hour, min, sec;
					struct tm *tm;
					verbose = overbose;
					sscanf(reply_string,
					    "%*s %04d%02d%02d%02d%02d%02d",
					    &yy, &mo, &day, &hour, &min, &sec);
					tm = gmtime(&stbuf.st_mtime);
					tm->tm_mon++;
					if (tm->tm_year > yy%100)
						return (1);
					else if (tm->tm_year == yy%100) {
						if (tm->tm_mon > mo)
							return (1);
					} else if (tm->tm_mon == mo) {
						if (tm->tm_mday > day)
							return (1);
					} else if (tm->tm_mday == day) {
						if (tm->tm_hour > hour)
							return (1);
					} else if (tm->tm_hour == hour) {
						if (tm->tm_min > min)
							return (1);
					} else if (tm->tm_min == min) {
						if (tm->tm_sec > sec)
							return (1);
					}
				} else {
					printf("%s\n", reply_string);
					verbose = overbose;
					return (0);
				}
			}
		}
	}

	recvrequest("RETR", argv[2], argv[1], mode,
	    argv[1] != oldargv1 || argv[2] != oldargv2);
	restart_point = 0;
	return (0);
}

void
mabort()
{
	int ointer;
	extern jmp_buf jabort;

	printf("\n");
	(void) fflush(stdout);
	if (mflag && fromatty) {
		ointer = interactive;
		interactive = 1;
		if (confirm("Continue with", mname)) {
			interactive = ointer;
			longjmp(jabort,0);
		}
		interactive = ointer;
	}
	mflag = 0;
	longjmp(jabort,0);
}

/*
 * Get multiple files.
 */
mget(argc, argv)
	int argc;
	char **argv;
{
	extern jmp_buf jabort;
	sig_t oldintr;
	int ointer;
	char *cp, *tp, *tp2, tmpbuf[MAXPATHLEN];
	void mabort();

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT,mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv,proxy)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			tp = cp;
			if (mcase) {
				while (*tp && !islower(*tp)) {
					tp++;
				}
				if (!*tp) {
					tp = cp;
					tp2 = tmpbuf;
					while ((*tp2 = *tp) != NULL) {
						if (isupper(*tp2)) {
							*tp2 = 'a' + *tp2 - 'A';
						}
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
			recvrequest("RETR", tp, cp, "w",
			    tp != cp || !interactive);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with","mget")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
	}
	(void) signal(SIGINT,oldintr);
	mflag = 0;
}

char *
remglob(argv,doswitch)
	char *argv[];
	int doswitch;
{
	char temp[16];
	static char buf[MAXPATHLEN];
	static FILE *ftemp = NULL;
	static char **args;
	int oldverbose, oldhash;
	char *cp, *mode;

	if (!mflag) {
		if (!doglob) {
			args = NULL;
		}
		else {
			if (ftemp) {
				(void) fclose(ftemp);
				ftemp = NULL;
			}
		}
		return(NULL);
	}
	if (!doglob) {
		if (args == NULL)
			args = argv;
		if ((cp = *++args) == NULL)
			args = NULL;
		return (cp);
	}
	if (ftemp == NULL) {
		(void) strcpy(temp, _PATH_TMP);
		(void) mktemp(temp);
		oldverbose = verbose, verbose = 0;
		oldhash = hash, hash = 0;
		if (doswitch) {
			pswitch(!proxy);
		}
		for (mode = "w"; *++argv != NULL; mode = "a")
			recvrequest ("NLST", temp, *argv, mode, 0);
		if (doswitch) {
			pswitch(!proxy);
		}
		verbose = oldverbose; hash = oldhash;
		ftemp = fopen(temp, "r");
		(void) unlink(temp);
		if (ftemp == NULL) {
			printf("can't find list of remote files, oops\n");
			return (NULL);
		}
	}
	if (fgets(buf, sizeof (buf), ftemp) == NULL) {
		(void) fclose(ftemp), ftemp = NULL;
		return (NULL);
	}
	if ((cp = index(buf, '\n')) != NULL)
		*cp = '\0';
	return (buf);
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
status(argc, argv)
	int argc;
	char *argv[];
{
	int i;

	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	if (!proxy) {
		pswitch(1);
		if (connected) {
			printf("Connected for proxy commands to %s.\n", hostname);
		}
		else {
			printf("No proxy connection.\n");
		}
		pswitch(0);
	}
	printf("Mode: %s; Type: %s; Form: %s; Structure: %s\n",
		modename, typename, formname, structname);
	printf("Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s\n", 
		onoff(verbose), onoff(bell), onoff(interactive),
		onoff(doglob));
	printf("Store unique: %s; Receive unique: %s\n", onoff(sunique),
		onoff(runique));
	printf("Case: %s; CR stripping: %s\n",onoff(mcase),onoff(crflag));
	if (ntflag) {
		printf("Ntrans: (in) %s (out) %s\n", ntin,ntout);
	}
	else {
		printf("Ntrans: off\n");
	}
	if (mapflag) {
		printf("Nmap: (in) %s (out) %s\n", mapin, mapout);
	}
	else {
		printf("Nmap: off\n");
	}
	printf("Hash mark printing: %s; Use of PORT cmds: %s\n",
		onoff(hash), onoff(sendport));
	if (macnum > 0) {
		printf("Macros:\n");
		for (i=0; i<macnum; i++) {
			printf("\t%s\n",macros[i].mac_name);
		}
	}
	code = 0;
}

/*
 * Set beep on cmd completed mode.
 */
/*VARARGS*/
setbell()
{

	bell = !bell;
	printf("Bell mode %s.\n", onoff(bell));
	code = bell;
}

/*
 * Turn on packet tracing.
 */
/*VARARGS*/
settrace()
{

	trace = !trace;
	printf("Packet tracing %s.\n", onoff(trace));
	code = trace;
}

/*
 * Toggle hash mark printing during transfers.
 */
/*VARARGS*/
sethash()
{

	hash = !hash;
	printf("Hash mark printing %s", onoff(hash));
	code = hash;
	if (hash)
		printf(" (%d bytes/hash mark)", 1024);
	printf(".\n");
}

/*
 * Turn on printing of server echo's.
 */
/*VARARGS*/
setverbose()
{

	verbose = !verbose;
	printf("Verbose mode %s.\n", onoff(verbose));
	code = verbose;
}

/*
 * Toggle PORT cmd use before each data connection.
 */
/*VARARGS*/
setport()
{

	sendport = !sendport;
	printf("Use of PORT cmds %s.\n", onoff(sendport));
	code = sendport;
}

/*
 * Turn on interactive prompting
 * during mget, mput, and mdelete.
 */
/*VARARGS*/
setprompt()
{

	interactive = !interactive;
	printf("Interactive mode %s.\n", onoff(interactive));
	code = interactive;
}

/*
 * Toggle metacharacter interpretation
 * on local file names.
 */
/*VARARGS*/
setglob()
{
	
	doglob = !doglob;
	printf("Globbing %s.\n", onoff(doglob));
	code = doglob;
}

/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
/*VARARGS*/
setdebug(argc, argv)
	int argc;
	char *argv[];
{
	int val;

	if (argc > 1) {
		val = atoi(argv[1]);
		if (val < 0) {
			printf("%s: bad debugging value.\n", argv[1]);
			code = -1;
			return;
		}
	} else
		val = !debug;
	debug = val;
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
cd(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "remote-directory")) {
		printf("usage: %s remote-directory\n", argv[0]);
		code = -1;
		return;
	}
	if (command("CWD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("CWD command not recognized, trying XCWD\n");
		(void) command("XCWD %s", argv[1]);
	}
}

/*
 * Set current working directory
 * on local machine.
 */
lcd(argc, argv)
	int argc;
	char *argv[];
{
	char buf[MAXPATHLEN];
	extern char *getwd();

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
		fprintf(stderr, "local: %s: %s\n", argv[1], strerror(errno));
		code = -1;
		return;
	}
	printf("Local directory now %s\n", getwd(buf));
	code = 0;
}

/*
 * Delete a single file.
 */
delete(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "remote-file")) {
		printf("usage: %s remote-file\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
mdelete(argc, argv)
	int argc;
	char **argv;
{
	extern jmp_buf jabort;
	sig_t oldintr;
	int ointer;
	char *cp;
	void mabort();

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv,0)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			(void) command("DELE %s", cp);
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
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Rename a remote file.
 */
renamefile(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "from-name"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "to-name")) {
usage:
		printf("%s from-name to-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RNFR %s", argv[1]) == CONTINUE)
		(void) command("RNTO %s", argv[2]);
}

/*
 * Get a directory listing
 * of remote files.
 */
ls(argc, argv)
	int argc;
	char *argv[];
{
	char *cmd;

	if (argc < 2)
		argc++, argv[1] = NULL;
	if (argc < 3)
		argc++, argv[2] = "-";
	if (argc > 3) {
		printf("usage: %s remote-directory local-file\n", argv[0]);
		code = -1;
		return;
	}
	cmd = argv[0][0] == 'n' ? "NLST" : "LIST";
	if (strcmp(argv[2], "-") && !globulize(&argv[2])) {
		code = -1;
		return;
	}
	if (strcmp(argv[2], "-") && *argv[2] != '|')
		if (!globulize(&argv[2]) || !confirm("output to local-file:", argv[2])) {
			code = -1;
			return;
	}
	recvrequest(cmd, argv[2], argv[1], "w", 0);
}

/*
 * Get a directory listing
 * of multiple remote files.
 */
mls(argc, argv)
	int argc;
	char **argv;
{
	extern jmp_buf jabort;
	sig_t oldintr;
	int ointer, i;
	char *cmd, mode[1], *dest;
	void mabort();

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
	cmd = argv[0][1] == 'l' ? "NLST" : "LIST";
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
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
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Do a shell escape
 */
/*ARGSUSED*/
shell(argc, argv)
	int argc;
	char **argv;
{
	int pid;
	sig_t old1, old2;
	char shellnam[40], *shell, *namep; 
	union wait status;

	old1 = signal (SIGINT, SIG_IGN);
	old2 = signal (SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		for (pid = 3; pid < 20; pid++)
			(void) close(pid);
		(void) signal(SIGINT, SIG_DFL);
		(void) signal(SIGQUIT, SIG_DFL);
		shell = getenv("SHELL");
		if (shell == NULL)
			shell = _PATH_BSHELL;
		namep = rindex(shell,'/');
		if (namep == NULL)
			namep = shell;
		(void) strcpy(shellnam,"-");
		(void) strcat(shellnam, ++namep);
		if (strcmp(namep, "sh") != 0)
			shellnam[0] = '+';
		if (debug) {
			printf ("%s\n", shell);
			(void) fflush (stdout);
		}
		if (argc > 1) {
			execl(shell,shellnam,"-c",altarg,(char *)0);
		}
		else {
			execl(shell,shellnam,(char *)0);
		}
		perror(shell);
		code = -1;
		exit(1);
		}
	if (pid > 0)
		while (wait((int *)&status) != pid)
			;
	(void) signal(SIGINT, old1);
	(void) signal(SIGQUIT, old2);
	if (pid == -1) {
		perror("Try again later");
		code = -1;
	}
	else {
		code = 0;
	}
	return (0);
}

/*
 * Send new user information (re-login)
 */
user(argc, argv)
	int argc;
	char **argv;
{
	char acct[80], *getpass();
	int n, aflag = 0;

	if (argc < 2)
		(void) another(&argc, &argv, "username");
	if (argc < 2 || argc > 4) {
		printf("usage: %s username [password] [account]\n", argv[0]);
		code = -1;
		return (0);
	}
	n = command("USER %s", argv[1]);
	if (n == CONTINUE) {
		if (argc < 3 )
			argv[2] = getpass("Password: "), argc++;
		n = command("PASS %s", argv[2]);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			printf("Account: "); (void) fflush(stdout);
			(void) fgets(acct, sizeof(acct) - 1, stdin);
			acct[strlen(acct) - 1] = '\0';
			argv[3] = acct; argc++;
		}
		n = command("ACCT %s", argv[3]);
		aflag++;
	}
	if (n != COMPLETE) {
		fprintf(stdout, "Login failed.\n");
		return (0);
	}
	if (!aflag && argc == 4) {
		(void) command("ACCT %s", argv[3]);
	}
	return (1);
}

/*
 * Print working directory.
 */
/*VARARGS*/
pwd()
{
	int oldverbose = verbose;

	/*
	 * If we aren't verbose, this doesn't do anything!
	 */
	verbose = 1;
	if (command("PWD") == ERROR && code == 500) {
		printf("PWD command not recognized, trying XPWD\n");
		(void) command("XPWD");
	}
	verbose = oldverbose;
}

/*
 * Make a directory.
 */
makedir(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "directory-name")) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("MKD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("MKD command not recognized, trying XMKD\n");
		(void) command("XMKD %s", argv[1]);
	}
}

/*
 * Remove a directory.
 */
removedir(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "directory-name")) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RMD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("RMD command not recognized, trying XRMD\n");
		(void) command("XRMD %s", argv[1]);
	}
}

/*
 * Send a line, verbatim, to the remote machine.
 */
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
quote1(initial, argc, argv)
	char *initial;
	int argc;
	char **argv;
{
	register int i, len;
	char buf[BUFSIZ];		/* must be >= sizeof(line) */

	(void) strcpy(buf, initial);
	if (argc > 1) {
		len = strlen(buf);
		len += strlen(strcpy(&buf[len], argv[1]));
		for (i = 2; i < argc; i++) {
			buf[len++] = ' ';
			len += strlen(strcpy(&buf[len], argv[i]));
		}
	}
	if (command(buf) == PRELIM) {
		while (getreply(0) == PRELIM);
	}
}

do_chmod(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "mode"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "file-name")) {
usage:
		printf("usage: %s mode file-name\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("SITE CHMOD %s %s", argv[1], argv[2]);
}

do_umask(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "SITE UMASK" : "SITE UMASK %s", argv[1]);
	verbose = oldverbose;
}

idle(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "SITE IDLE" : "SITE IDLE %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Ask the other side for help.
 */
rmthelp(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "HELP" : "HELP %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
/*VARARGS*/
quit()
{

	if (connected)
		disconnect();
	pswitch(1);
	if (connected) {
		disconnect();
	}
	exit(0);
}

/*
 * Terminate session, but don't exit.
 */
disconnect()
{
	extern FILE *cout;
	extern int data;

	if (!connected)
		return;
	(void) command("QUIT");
	if (cout) {
		(void) fclose(cout);
	}
	cout = NULL;
	connected = 0;
	data = -1;
	if (!proxy) {
		macnum = 0;
	}
}

confirm(cmd, file)
	char *cmd, *file;
{
	char line[BUFSIZ];

	if (!interactive)
		return (1);
	printf("%s %s? ", cmd, file);
	(void) fflush(stdout);
	if (fgets(line, sizeof line, stdin) == NULL)
		return (0);
	return (*line != 'n' && *line != 'N');
}

fatal(msg)
	char *msg;
{

	fprintf(stderr, "ftp: %s\n", msg);
	exit(1);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
globulize(cpp)
	char **cpp;
{
	char **globbed;

	if (!doglob)
		return (1);
	globbed = ftpglob(*cpp);
	if (globerr != NULL) {
		printf("%s: %s\n", *cpp, globerr);
		if (globbed) {
			blkfree(globbed);
			free((char *)globbed);
		}
		return (0);
	}
	if (globbed) {
		*cpp = *globbed++;
		/* don't waste too much memory */
		if (*globbed) {
			blkfree(globbed);
			free((char *)globbed);
		}
	}
	return (1);
}

account(argc,argv)
	int argc;
	char **argv;
{
	char acct[50], *getpass(), *ap;

	if (argc > 1) {
		++argv;
		--argc;
		(void) strncpy(acct,*argv,49);
		acct[49] = '\0';
		while (argc > 1) {
			--argc;
			++argv;
			(void) strncat(acct,*argv, 49-strlen(acct));
		}
		ap = acct;
	}
	else {
		ap = getpass("Account:");
	}
	(void) command("ACCT %s", ap);
}

jmp_buf abortprox;

void
proxabort()
{
	extern int proxy;

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
	longjmp(abortprox,1);
}

doproxy(argc,argv)
	int argc;
	char *argv[];
{
	extern struct cmd cmdtab[];
	extern jmp_buf abortprox;
	register struct cmd *c;
	struct cmd *getcmd();
	sig_t oldintr;
	void proxabort();

	if (argc < 2 && !another(&argc, &argv, "command")) {
		printf("usage: %s command\n", argv[0]);
		code = -1;
		return;
	}
	c = getcmd(argv[1]);
	if (c == (struct cmd *) -1) {
		printf("?Ambiguous command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (c == 0) {
		printf("?Invalid command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (!c->c_proxy) {
		printf("?Invalid proxy command\n");
		(void) fflush(stdout);
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
		printf("Not connected\n");
		(void) fflush(stdout);
		pswitch(0);
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	(*c->c_handler)(argc-1, argv+1);
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	(void) signal(SIGINT, oldintr);
}

setcase()
{
	mcase = !mcase;
	printf("Case mapping %s.\n", onoff(mcase));
	code = mcase;
}

setcr()
{
	crflag = !crflag;
	printf("Carriage Return stripping %s.\n", onoff(crflag));
	code = crflag;
}

setntrans(argc,argv)
	int argc;
	char *argv[];
{
	if (argc == 1) {
		ntflag = 0;
		printf("Ntrans off.\n");
		code = ntflag;
		return;
	}
	ntflag++;
	code = ntflag;
	(void) strncpy(ntin, argv[1], 16);
	ntin[16] = '\0';
	if (argc == 2) {
		ntout[0] = '\0';
		return;
	}
	(void) strncpy(ntout, argv[2], 16);
	ntout[16] = '\0';
}

char *
dotrans(name)
	char *name;
{
	static char new[MAXPATHLEN];
	char *cp1, *cp2 = new;
	register int i, ostop, found;

	for (ostop = 0; *(ntout + ostop) && ostop < 16; ostop++);
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
	return(new);
}

setnmap(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;

	if (argc == 1) {
		mapflag = 0;
		printf("Nmap off.\n");
		code = mapflag;
		return;
	}
	if (argc < 3 && !another(&argc, &argv, "mapout")) {
		printf("Usage: %s [mapin mapout]\n",argv[0]);
		code = -1;
		return;
	}
	mapflag = 1;
	code = 1;
	cp = index(altarg, ' ');
	if (proxy) {
		while(*++cp == ' ');
		altarg = cp;
		cp = index(altarg, ' ');
	}
	*cp = '\0';
	(void) strncpy(mapin, altarg, MAXPATHLEN - 1);
	while (*++cp == ' ');
	(void) strncpy(mapout, cp, MAXPATHLEN - 1);
}

char *
domap(name)
	char *name;
{
	static char new[MAXPATHLEN];
	register char *cp1 = name, *cp2 = mapin;
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
				if (*++cp2 == '$' && isdigit(*(cp2+1))) { 
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
   						        isdigit(*(cp2+1))) {
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
						printf("nmap: unbalanced brackets\n");
						return(name);
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
						printf("nmap: unbalanced brackets\n");
						return(name);
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
				if (isdigit(*(cp2 + 1))) {
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
		return(name);
	}
	return(new);
}

setsunique()
{
	sunique = !sunique;
	printf("Store unique %s.\n", onoff(sunique));
	code = sunique;
}

setrunique()
{
	runique = !runique;
	printf("Receive unique %s.\n", onoff(runique));
	code = runique;
}

/* change directory to perent directory */
cdup()
{
	if (command("CDUP") == ERROR && code == 500) {
		if (verbose)
			printf("CDUP command not recognized, trying XCUP\n");
		(void) command("XCUP");
	}
}

/* restart transfer at specific point */
restart(argc, argv)
	int argc;
	char *argv[];
{
	extern long atol();
	if (argc != 2)
		printf("restart: offset not specified\n");
	else {
		restart_point = atol(argv[1]);
		printf("restarting at %ld. %s\n", restart_point,
		    "execute get, put or append to initiate transfer");
	}
}

/* show remote system type */
syst()
{
	(void) command("SYST");
}

macdef(argc, argv)
	int argc;
	char *argv[];
{
	char *tmp;
	int c;

	if (macnum == 16) {
		printf("Limit of 16 macros have already been defined\n");
		code = -1;
		return;
	}
	if (argc < 2 && !another(&argc, &argv, "macro name")) {
		printf("Usage: %s macro_name\n",argv[0]);
		code = -1;
		return;
	}
	if (interactive) {
		printf("Enter macro line by line, terminating it with a null line\n");
	}
	(void) strncpy(macros[macnum].mac_name, argv[1], 8);
	if (macnum == 0) {
		macros[macnum].mac_start = macbuf;
	}
	else {
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	}
	tmp = macros[macnum].mac_start;
	while (tmp != macbuf+4096) {
		if ((c = getchar()) == EOF) {
			printf("macdef:end of file encountered\n");
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
			printf("Macro not defined - 4k buffer exceeded\n");
			code = -1;
			return;
		}
	}
}

/*
 * get size of file on remote machine
 */
sizecmd(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "filename")) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("SIZE %s", argv[1]);
}

/*
 * get last modification time of file on remote machine
 */
modtime(argc, argv)
	int argc;
	char *argv[];
{
	int overbose;

	if (argc < 2 && !another(&argc, &argv, "filename")) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	overbose = verbose;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", argv[1]) == COMPLETE) {
		int yy, mo, day, hour, min, sec;
		sscanf(reply_string, "%*s %04d%02d%02d%02d%02d%02d", &yy, &mo,
			&day, &hour, &min, &sec);
		/* might want to print this in local time */
		printf("%s\t%02d/%02d/%04d %02d:%02d:%02d GMT\n", argv[1],
			mo, day, yy, hour, min, sec);
	} else
		printf("%s\n", reply_string);
	verbose = overbose;
}

/*
 * show status on reomte machine
 */
rmtstatus(argc, argv)
	int argc;
	char *argv[];
{
	(void) command(argc > 1 ? "STAT %s" : "STAT" , argv[1]);
}

/*
 * get file if modtime is more recent than current file
 */
newer(argc, argv)
	int argc;
	char *argv[];
{
	if (getit(argc, argv, -1, "w"))
		printf("Local file \"%s\" is newer than remote file \"%s\"\n",
			argv[1], argv[2]);
}
