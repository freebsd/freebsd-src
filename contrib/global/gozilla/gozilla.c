/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	gozilla.c				27-Oct-97
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "global.h"

char	*progname  = "gozilla";		/* command name */

static void	usage __P((void));
void	main __P((int, char **));
int	sendcommand __P((char *));

int	bflag;
int	pflag;
int	Cflag;

static void
usage()
{
	fprintf(stderr, "usage:\t%s\n\t%s\n",
		"gozilla [+no] [-b browser] [-p] file",
		"gozilla -C command");
	exit(1);
}

void
main(argc, argv)
int	argc;
char	*argv[];
{
	char	c, *p, *q;
	char	*browser = (char *)0;
	char	*command = (char *)0;
	char	*arg     = (char *)0;
	char	URL[MAXPATHLEN+1];
	char	com[MAXCOMLINE+1];
	int	linenumber = 0;
	int	status;

	while (--argc > 0 && (c = (++argv)[0][0]) == '-' || c == '+') {
		if (c == '+') {
			linenumber = atoi(argv[0] + 1);
			continue;
		}
		p = argv[0] + 1;
		switch (*p) {
		case 'b':
			browser = argv[1];
			--argc; ++argv;
			break;
		case 'p':
			pflag++;
			break;
		case 'C':
			command = argv[1];
			--argc; ++argv;
			break;
		default:
			usage();
		}
	}
	if (!browser && getenv("BROWSER"))
		browser = getenv("BROWSER");
	if (command) {
		if (browser)
			die("-C option is valid only for mozilla.");
		if (sendcommand(command) == -1)
			die("mozilla not found.");
		exit(0);
	}
	if (argc == 0)
		usage();
	if (locatestring(argv[0], "http:", 1) || locatestring(argv[0], "file:", 1))
		strcpy(URL, argv[0]);
	else {
		char	*abspath;
		char	pathbuf[MAXPATHLEN+1];
		char	htmlpath[MAXPATHLEN+1];

		if (!test("f", argv[0]) && !test("d", argv[0]))
			die1("path '%s' not found.", argv[0]);
		if (!(abspath = realpath(argv[0], pathbuf)))
			die1("cannot make absolute path name. realpath(%s) failed.", argv[0]);
		if (*abspath != '/')
			die("realpath(3) is not compatible with BSD version.");
		if (issource(abspath)) {
			char	cwd[MAXPATHLEN+1];
			char	root[MAXPATHLEN+1];
			char	dbpath[MAXPATHLEN+1];
			char	htmldir[MAXPATHLEN+1];
			/*
			 * get current, root and dbpath directory.
			 * if GTAGS not found, getdbpath doesn't return.
			 */
			getdbpath(cwd, root, dbpath);
			if (test("d", makepath(dbpath, "HTML")))
				strcpy(htmldir, makepath(dbpath, "HTML"));
			else if (test("d", makepath(root, "HTML")))
				strcpy(htmldir, makepath(root, "HTML"));
			else
				die("hypertext not found. See htags(1).");
			/*
			 * convert path into hypertext.
			 */
			p = abspath + strlen(root);
			for (q = ++p; *q; q++)
				if (*q == '/')
					*q = ' ';
			if (linenumber)
				sprintf(URL, "file:%s/S/%s.html#%d", htmldir, p, linenumber);
			else
				sprintf(URL, "file:%s/S/%s.html", htmldir, p);
		} else {
			sprintf(URL, "file:%s", abspath);
		}
	}
	if (pflag) {
		fprintf(stdout, "%s\n", URL);
		exit(0);
	}
	/*
	 * execute generic browser.
	 */
	if (browser && !locatestring(browser, "netscape", 3)) {
		sprintf(com, "%s '%s'", browser, URL);
		system(com);
		exit (0);
	}
	/*
	 * send a command to mozilla.
	 */
	sprintf(com, "openURL(%s)", URL);
	status = sendcommand(com);
	/*
	 * load mozilla if not found.
	 */
	if (status != 0) {
		int	pid;

		if ((pid = fork()) < 0) {
			die("cannot execute netscape (fork).");
		} else if (pid == 0) {
			execlp("netscape", "netscape", URL, (char *)0);
			die("loading mozilla failed.");
		}
		exit(0);
	}
	exit(status);
}
int
sendcommand(com)
char	*com;
{
	int	argc = 3;
	char	*argv[4];

	argv[0] = "netscape-remote";
	argv[1] = "-remote";
	argv[2] = com;
	argv[3] = (char *)0;

	return netscape_remote(argc, argv);
}
