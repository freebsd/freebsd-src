/*
 * Copyright (c) 1983, 1986, 1987 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983, 1986, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)bugfiler.c	5.16 (Berkeley) 2/25/91";
#endif /* not lint */

/*
 * Bug report processing program, designed to be invoked
 * through aliases(5).
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bug.h"

char	bfr[MAXBSIZE],			/* general I/O buffer */
	tmpname[sizeof(TMP_BUG) + 5];	/* temp bug file */

main(argc, argv)
	int	argc;
	char	**argv;
{
	extern char	*optarg;	/* getopt arguments */
	register struct passwd	*pwd;	/* bugs password entry */
	register int	ch;		/* getopts char */
	int	do_ack,			/* acknowledge bug report */
		do_redist;		/* redistribut BR */
	char	*argversion;		/* folder name provided */
	static void logit(), make_copy();

	do_ack = do_redist = YES;
	argversion = NULL;
	while ((ch = getopt(argc, argv, "av:r")) != EOF)
		switch((char)ch) {
		case 'a':
			do_ack = NO;
			break;
		case 'v':
			argversion = optarg;
			break;
		case 'r':
			do_redist = NO;
			break;
		case '?':
		default:
			fputs("usage: bugfiler [-ar] [-v version]\n", stderr);
			error("usage: bugfiler [-ar] [-v version]", CHN);
		}

	if (!(pwd = getpwnam(BUGS_ID)))
		error("can't find bugs login.", BUGS_ID);

	if (chdir(pwd->pw_dir))		/* change to bugs home directory */
		error("can't chdir to %s.", pwd->pw_dir);

	if (setreuid(0, pwd->pw_uid))
		error("can't set id to %s.", BUGS_ID);

	(void)umask(02);		/* everything is 664 */
	seterr();			/* redirect to log file */
	logit();			/* log report arrival */
	make_copy();			/* save copy in case */
	gethead(do_redist);

	if (argversion)			/* specific folder requested */
		(void)strcpy(dir, argversion);

	process();

	if (setuid(0))
		error("can't set id to root.", CHN);
	if (do_ack)
		reply();
	if (do_redist)
		redist();
	(void)unlink(tmpname);
	exit(OK);
}

/*
 * make_copy --
 *	make a copy of bug report in error folder
 */
static void
make_copy()
{
	register int	cnt,			/* read return value */
			tfd;			/* temp file descriptor */
	char	*strcpy();

	if (access(TMP_DIR, F_OK))
		(void)mkdir(TMP_DIR, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
	(void)strcpy(tmpname, TMP_BUG);
	if (tfd = mkstemp(tmpname)) {
		while ((cnt = read(fileno(stdin), bfr, sizeof(bfr))) != ERR && cnt)
			write(tfd, bfr, cnt);
		(void)close(tfd);
		return;
	}
	error("can't make copy using %s.", tmpname);
}

/*
 * logit --
 *	log this run of the bugfiler
 */
static void
logit()
{
	struct timeval	tp;
	char	*C1, *C2,
		*ctime();

	if (gettimeofday(&tp, (struct timezone *)NULL))
		error("can't get time of day.", CHN);
	for (C1 = C2 = ctime(&tp.tv_sec); *C1 && *C1 != '\n'; ++C1);
	*C1 = EOS;
	fputs(C2, stderr);
}
