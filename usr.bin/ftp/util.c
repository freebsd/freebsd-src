/* $FreeBSD$	*/
/*	$NetBSD: util.c,v 1.16.2.1 1997/11/18 01:02:33 mellon Exp $	*/

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
__RCSID("$FreeBSD$");
__RCSID_SOURCE("$NetBSD: util.c,v 1.16.2.1 1997/11/18 01:02:33 mellon Exp $");
#endif /* not lint */

/*
 * FTP User Program -- Misc support routines
 */
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef INET6
#include <netdb.h>
#endif

#include "ftp_var.h"
#include "pathnames.h"

#ifndef	SECSPERHOUR
#define	SECSPERHOUR	(60*60)
#endif

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	char *host;
	char *port;

	if (connected) {
		printf("Already connected to %s, use close first.\n",
		    hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void)another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
		printf("usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	if (gatemode)
		port = gateport;
	else
		port = ftpport;
	if (argc > 2)
		port = strdup(argv[2]);

	if (gatemode) {
		if (gateserver == NULL || *gateserver == '\0')
			errx(1, "gateserver not defined (shouldn't happen)");
		host = hookup(gateserver, port);
	} else
		host = hookup(argv[1], port);

	if (host) {
		int overbose;

		if (gatemode) {
			if (command("PASSERVE %s", argv[1]) != COMPLETE)
				return;
			if (verbose)
				printf("Connected via pass-through server %s\n",
				    gateserver);
		}

		connected = 1;
		try_epsv = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void)strcpy(typename, "ascii"), type = TYPE_A;
		curtype = TYPE_A;
		(void)strcpy(formname, "non-print"), form = FORM_N;
		(void)strcpy(modename, "stream"), mode = MODE_S;
		(void)strcpy(structname, "file"), stru = STRU_F;
		(void)strcpy(bytename, "8"), bytesize = 8;
		if (autologin)
			(void)login(argv[1], NULL, NULL);

		overbose = verbose;
		if (debug == 0)
			verbose = -1;
		if (command("SYST") == COMPLETE && overbose) {
			char *cp, c;
			c = 0;
			cp = strchr(reply_string+4, ' ');
			if (cp == NULL)
				cp = strchr(reply_string+4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			printf("Remote system type is %s.\n", reply_string + 4);
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
			(void)strcpy(typename, "binary");
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
				puts(
"Remember to set tenex mode when transferring binary files from this machine.");
		}
		verbose = overbose;
	}
}


/*
 * login to remote host, using given username & password if supplied
 */
int
login(host, user, pass)
	const char *host;
	char *user, *pass;
{
	char tmp[80];
	char *acct;
	char anonpass[MAXLOGNAME + 1 + MAXHOSTNAMELEN];	/* "user@hostname" */
	char hostname[MAXHOSTNAMELEN];
	struct passwd *pw;
	int n, aflag = 0;

	acct = NULL;
	if (user == NULL) {
		if (ruserpass(host, &user, &pass, &acct) < 0) {
			code = -1;
			return (0);
		}
	}

	/*
	 * Set up arguments for an anonymous FTP session, if necessary.
	 */
	if ((user == NULL || pass == NULL) && anonftp) {
		memset(anonpass, 0, sizeof(anonpass));
		memset(hostname, 0, sizeof(hostname));

		/*
		 * Set up anonymous login password.
		 */
		if ((user = getlogin()) == NULL) {
			if ((pw = getpwuid(getuid())) == NULL)
				user = "anonymous";
			else
				user = pw->pw_name;
		}
		gethostname(hostname, MAXHOSTNAMELEN);
#ifndef DONT_CHEAT_ANONPASS
		/*
		 * Every anonymous FTP server I've encountered
		 * will accept the string "username@", and will
		 * append the hostname itself.  We do this by default
		 * since many servers are picky about not having
		 * a FQDN in the anonymous password. - thorpej@netbsd.org
		 */
		snprintf(anonpass, sizeof(anonpass) - 1, "%s@",
		    user);
#else
		snprintf(anonpass, sizeof(anonpass) - 1, "%s@%s",
		    user, hp->h_name);
#endif
		pass = anonpass;
		user = "anonymous";	/* as per RFC 1635 */
	}

	while (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL && (pw = getpwuid(getuid())) != NULL)
			myname = pw->pw_name;
		if (myname)
			printf("Name (%s:%s): ", host, myname);
		else
			printf("Name (%s): ", host);
		if (fgets(tmp, sizeof(tmp) - 1, stdin) == NULL)
			return (0);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0')
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = getpass("Password:");
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL)
			acct = getpass("Account:");
		n = command("ACCT %s", acct);
	}
	if ((n != COMPLETE) ||
	    (!aflag && acct != NULL && command("ACCT %s", acct) != COMPLETE)) {
		warnx("Login failed.");
		return (0);
	}
	if (proxy)
		return (1);
	connected = -1;
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void)strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

/*
 * `another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another(pargc, pargv, prompt)
	int *pargc;
	char ***pargv;
	const char *prompt;
{
	int len = strlen(line), ret;

	if (len >= sizeof(line) - 3) {
		puts("sorry, arguments too long.");
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
 * glob files given in argv[] from the remote server.
 * if errbuf isn't NULL, store error messages there instead
 * of writing to the screen.
 */
char *
remglob(argv, doswitch, errbuf)
        char *argv[];
        int doswitch;
	char **errbuf;
{
        char temp[MAXPATHLEN];
        static char buf[MAXPATHLEN];
        static FILE *ftemp = NULL;
        static char **args;
        int oldverbose, oldhash, fd;
        char *cp, *mode;

        if (!mflag) {
                if (!doglob)
                        args = NULL;
                else {
                        if (ftemp) {
                                (void)fclose(ftemp);
                                ftemp = NULL;
                        }
                }
                return (NULL);
        }
        if (!doglob) {
                if (args == NULL)
                        args = argv;
                if ((cp = *++args) == NULL)
                        args = NULL;
                return (cp);
        }
        if (ftemp == NULL) {
                (void)snprintf(temp, sizeof(temp), "%s/%s", tmpdir, TMPFILE);
                if ((fd = mkstemp(temp)) < 0) {
                        warn("unable to create temporary file %s", temp);
                        return (NULL);
                }
                close(fd);
                oldverbose = verbose;
		verbose = (errbuf != NULL) ? -1 : 0;
                oldhash = hash;
                hash = 0;
                if (doswitch)
                        pswitch(!proxy);
                for (mode = "w"; *++argv != NULL; mode = "a")
                        recvrequest("NLST", temp, *argv, mode, 0, 0);
		if ((code / 100) != COMPLETE) {
			if (errbuf != NULL)
				*errbuf = reply_string;
		}
                if (doswitch)
                        pswitch(!proxy);
                verbose = oldverbose;
		hash = oldhash;
                ftemp = fopen(temp, "r");
                (void)unlink(temp);
                if (ftemp == NULL) {
			if (errbuf == NULL)
				puts("can't find list of remote files, oops.");
			else
				*errbuf =
				    "can't find list of remote files, oops.";
                        return (NULL);
                }
        }
        if (fgets(buf, sizeof(buf), ftemp) == NULL) {
                (void)fclose(ftemp);
		ftemp = NULL;
                return (NULL);
        }
        if ((cp = strchr(buf, '\n')) != NULL)
                *cp = '\0';
        return (buf);
}

int
confirm(cmd, file)
	const char *cmd, *file;
{
	char line[BUFSIZ];

	if (!interactive || confirmrest)
		return (1);
	printf("%s %s? ", cmd, file);
	(void)fflush(stdout);
	if (fgets(line, sizeof(line), stdin) == NULL)
		return (0);
	switch (tolower((unsigned char)*line)) {
		case 'n':
			return (0);
		case 'p':
			interactive = 0;
			puts("Interactive mode: off.");
			break;
		case 'a':
			confirmrest = 1;
			printf("Prompting off for duration of %s.\n", cmd);
			break;
	}
	return (1);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
int
globulize(cpp)
	char **cpp;
{
	glob_t gl;
	int flags;

	if (!doglob)
		return (1);

	flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;
	memset(&gl, 0, sizeof(gl));
	if (glob(*cpp, flags, NULL, &gl) ||
	    gl.gl_pathc == 0) {
		warnx("%s: not found", *cpp);
		globfree(&gl);
		return (0);
	}
		/* XXX: caller should check if *cpp changed, and
		 *	free(*cpp) if that is the case
		 */
	*cpp = strdup(gl.gl_pathv[0]);
	globfree(&gl);
	return (1);
}

/*
 * determine size of remote file
 */
off_t
remotesize(file, noisy)
	const char *file;
	int noisy;
{
	int overbose;
	off_t size;

	overbose = verbose;
	size = -1;
	if (debug == 0)
		verbose = -1;
	if (command("SIZE %s", file) == COMPLETE) {
		char *cp, *ep;

		cp = strchr(reply_string, ' ');
		if (cp != NULL) {
			cp++;
			size = strtoq(cp, &ep, 10);
			if (*ep != '\0' && !isspace((unsigned char)*ep))
				size = -1;
		}
	} else if (noisy && debug == 0)
		puts(reply_string);
	verbose = overbose;
	return (size);
}

/*
 * determine last modification time (in GMT) of remote file
 */
time_t
remotemodtime(file, noisy)
	const char *file;
	int noisy;
{
	struct tm timebuf;
	time_t rtime;
	int len, month, ocode, overbose, y2kbug, year;
	char *fmt;
	char mtbuf[17];

	overbose = verbose;
	ocode = code;
	rtime = -1;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", file) == COMPLETE) {
		memset(&timebuf, 0, sizeof(timebuf));
		/*
		 * Parse the time string, which is expected to be 14
		 * characters long.  Some broken servers send tm_year
		 * formatted with "19%02d", which produces an incorrect
		 * (but parsable) 15 characters for years >= 2000.
		 * Scan for invalid trailing junk by accepting up to 16
		 * characters.
		 */
		if (sscanf(reply_string, "%*s %16s", mtbuf) == 1) {
			fmt = NULL;
			len = strlen(mtbuf);
			y2kbug = 0;
			if (len == 15 && strncmp(mtbuf, "19", 2) == 0) {
				fmt = "19%03d%02d%02d%02d%02d%02d";
				y2kbug = 1;
			} else if (len == 14)
				fmt = "%04d%02d%02d%02d%02d%02d";
			if (fmt != NULL) {
				if (sscanf(mtbuf, fmt, &year, &month,
				    &timebuf.tm_mday, &timebuf.tm_hour,
				    &timebuf.tm_min, &timebuf.tm_sec) == 6) {
					timebuf.tm_isdst = -1;
					timebuf.tm_mon = month - 1;
					if (y2kbug)
						timebuf.tm_year = year;
					else
						timebuf.tm_year = year - 1900;
					rtime = mktime(&timebuf);
				}
			}
		}
		if (rtime == -1) {
			if (noisy || debug != 0)
				printf("Can't convert %s to a time.\n", mtbuf);
		} else
			rtime += timebuf.tm_gmtoff;	/* conv. local -> GMT */
	} else if (noisy && debug == 0)
		puts(reply_string);
	verbose = overbose;
	if (rtime == -1)
		code = ocode;
	return (rtime);
}

void updateprogressmeter __P((int));

void
updateprogressmeter(dummy)
	int dummy;
{
	static pid_t pgrp = -1;
	int ctty_pgrp;

	if (pgrp == -1)
		pgrp = getpgrp();

	/*
	 * print progress bar only if we are foreground process.
	 */
	if (ioctl(STDOUT_FILENO, TIOCGPGRP, &ctty_pgrp) != -1 &&
	    ctty_pgrp == (int)pgrp)
		progressmeter(0);
}

/*
 * Display a transfer progress bar if progress is non-zero.
 * SIGALRM is hijacked for use by this function.
 * - Before the transfer, set filesize to size of file (or -1 if unknown),
 *   and call with flag = -1. This starts the once per second timer,
 *   and a call to updateprogressmeter() upon SIGALRM.
 * - During the transfer, updateprogressmeter will call progressmeter
 *   with flag = 0
 * - After the transfer, call with flag = 1
 */
static struct timeval start;

void
progressmeter(flag)
	int flag;
{
	/*
	 * List of order of magnitude prefixes.
	 * The last is `P', as 2^64 = 16384 Petabytes
	 */
	static const char prefixes[] = " KMGTP";

	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, len;
	off_t remaining;
	char buf[256];

	len = 0;

	if (flag == -1) {
		(void)gettimeofday(&start, (struct timezone *)0);
		lastupdate = start;
		lastsize = restart_point;
	}
	(void)gettimeofday(&now, (struct timezone *)0);
	if (!progress || filesize <= 0)
		return;
	cursize = bytes + restart_point;

	ratio = cursize * 100 / filesize;
	ratio = MAX(ratio, 0);
	ratio = MIN(ratio, 100);
	len += snprintf(buf + len, sizeof(buf) - len, "\r%3d%% ", ratio);

	barlength = ttywidth - 30;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		len += snprintf(buf + len, sizeof(buf) - len,
		    "|%.*s%*s|", i, 
"*****************************************************************************"
"*****************************************************************************",
		    barlength - i, "");
	}

	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	len += snprintf(buf + len, sizeof(buf) - len,
	    " %5qd %c%c ", (long long)abbrevsize, prefixes[i],
	    prefixes[i] == ' ' ? ' ' : 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {	/* fudge out stalled time */
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}

	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (bytes <= 0 || elapsed <= 0.0 || cursize > filesize) {
		len += snprintf(buf + len, sizeof(buf) - len,
		    "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		len += snprintf(buf + len, sizeof(buf) - len,
		    " - stalled -");
	} else {
		remaining = 
		    ((filesize - restart_point) / (bytes / elapsed) - elapsed);
		if (remaining >= 100 * SECSPERHOUR)
			len += snprintf(buf + len, sizeof(buf) - len,
			    "   --:-- ETA");
		else {
			i = remaining / SECSPERHOUR;
			if (i)
				len += snprintf(buf + len, sizeof(buf) - len,
				    "%2d:", i);
			else
				len += snprintf(buf + len, sizeof(buf) - len,
				    "   ");
			i = remaining % SECSPERHOUR;
			len += snprintf(buf + len, sizeof(buf) - len,
			    "%02d:%02d ETA", i / 60, i % 60);
		}
	}
	(void)write(STDOUT_FILENO, buf, len);

	if (flag == -1) {
		(void)signal(SIGALRM, updateprogressmeter);
		alarmtimer(1);		/* set alarm timer for 1 Hz */
	} else if (flag == 1) {
		alarmtimer(0);
		(void)putchar('\n');
	}
	fflush(stdout);
}

/*
 * Display transfer statistics.
 * Requires start to be initialised by progressmeter(-1),
 * direction to be defined by xfer routines, and filesize and bytes
 * to be updated by xfer routines
 * If siginfo is nonzero, an ETA is displayed, and the output goes to STDERR
 * instead of STDOUT.
 */
void
ptransfer(siginfo)
	int siginfo;
{
	struct timeval now, td;
	double elapsed;
	off_t bs;
	int meg, remaining, hh, len;
	char buf[100];

	if (!verbose && !siginfo)
		return;

	(void)gettimeofday(&now, (struct timezone *)0);
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);
	bs = bytes / (elapsed == 0.0 ? 1 : elapsed);
	meg = 0;
	if (bs > (1024 * 1024))
		meg = 1;
	len = 0;
	len += snprintf(buf + len, sizeof(buf) - len,
	    "%qd byte%s %s in %.2f seconds (%.2f %sB/s)\n",
	    (long long)bytes, bytes == 1 ? "" : "s", direction, elapsed,
	    bs / (1024.0 * (meg ? 1024.0 : 1.0)), meg ? "M" : "K");
	if (siginfo && bytes > 0 && elapsed > 0.0 && filesize >= 0
	    && bytes + restart_point <= filesize) {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		hh = remaining / SECSPERHOUR;
		remaining %= SECSPERHOUR;
		len--;	 		/* decrement len to overwrite \n */
		len += snprintf(buf + len, sizeof(buf) - len,
		    "  ETA: %02d:%02d:%02d\n", hh, remaining / 60,
		    remaining % 60);
	}
	(void)write(siginfo ? STDERR_FILENO : STDOUT_FILENO, buf, len);
}

/*
 * List words in stringlist, vertically arranged
 */
void
list_vertical(sl)
	StringList *sl;
{
	int i, j, w;
	int columns, width, lines, items;
	char *p;

	width = items = 0;

	for (i = 0 ; i < sl->sl_cur ; i++) {
		w = strlen(sl->sl_str[i]);
		if (w > width)
			width = w;
	}
	width = (width + 8) &~ 7;

	columns = ttywidth / width;
	if (columns == 0)
		columns = 1;
	lines = (sl->sl_cur + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			p = sl->sl_str[j * lines + i];
			if (p)
				fputs(p, stdout);
			if (j * lines + i + lines >= sl->sl_cur) {
				putchar('\n');
				break;
			}
			w = strlen(p);
			while (w < width) {
				w = (w + 8) &~ 7;
				(void)putchar('\t');
			}
		}
	}
}

/*
 * Update the global ttywidth value, using TIOCGWINSZ.
 */
void
setttywidth(a)
	int a;
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		ttywidth = winsize.ws_col;
	else
		ttywidth = 80;
}

/*
 * Set the SIGALRM interval timer for wait seconds, 0 to disable.
 */
void
alarmtimer(wait)
	int wait;
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

/*
 * Setup or cleanup EditLine structures
 */
#ifndef SMALL
void
controlediting()
{
	if (editing && el == NULL && hist == NULL) {
		el = el_init(__progname, stdin, stdout); /* init editline */
		hist = history_init();		/* init the builtin history */
		history(hist, H_EVENT, 100);	/* remember 100 events */
		el_set(el, EL_HIST, history, hist);	/* use history */

		el_set(el, EL_EDITOR, "emacs");	/* default editor is emacs */
		el_set(el, EL_PROMPT, prompt);	/* set the prompt function */

		/* add local file completion, bind to TAB */
		el_set(el, EL_ADDFN, "ftp-complete",
		    "Context sensitive argument completion",
		    complete);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);

		el_source(el, NULL);	/* read ~/.editrc */
		el_set(el, EL_SIGNAL, 1);
	} else if (!editing) {
		if (hist) {
			history_end(hist);
			hist = NULL;
		}
		if (el) {
			el_end(el);
			el = NULL;
		}
	}
}
#endif /* !SMALL */

/*
 * Determine if given string is an IPv6 address or not.
 * Return 1 for yes, 0 for no
 */
int
isipv6addr(const char *addr)
{
	int rv = 0;
#ifdef INET6
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(addr, "0", &hints, &res) != 0)
		rv = 0;
	else {
		rv = 1;
		freeaddrinfo(res);
	}
	if (debug)
		printf("isipv6addr: got %d for %s\n", rv, addr);
#endif
	return (rv == 1) ? 1 : 0;
}
