/*
 * Copyright (c) 1983 Regents of the University of California.
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
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cfscores.c	5.6 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <pwd.h>
#include "pathnames.h"

struct betinfo {
	long	hand;		/* cost of dealing hand */
	long	inspection;	/* cost of inspecting hand */
	long	game;		/* cost of buying game */
	long	runs;		/* cost of running through hands */
	long	information;	/* cost of information */
	long	thinktime;	/* cost of thinking time */
	long	wins;		/* total winnings */
	long	worth;		/* net worth after costs */
};

int dbfd;

main(argc, argv)
	int argc;
	char *argv[];
{
	register struct passwd *pw;
	int uid;

	if (argc > 2) {
		printf("Usage: cfscores [user]\n");
		exit(1);
	}
	dbfd = open(_PATH_SCORE, 0);
	if (dbfd < 0) {
		perror(_PATH_SCORE);
		exit(2);
	}
	setpwent();
	if (argc == 1) {
		uid = getuid();
		pw = getpwuid(uid);
		if (pw == 0) {
			printf("You are not listed in the password file?!?\n");
			exit(2);
		}
		printuser(pw, 1);
		exit(0);
	}
	if (strcmp(argv[1], "-a") == 0) {
		while ((pw = getpwent()) != 0)
			printuser(pw, 0);
		exit(0);
	}
	pw = getpwnam(argv[1]);
	if (pw == 0) {
		printf("User %s unknown\n", argv[1]);
		exit(3);
	}
	printuser(pw, 1);
	exit(0);
}

/*
 * print out info for specified password entry
 */
printuser(pw, printfail)
	register struct passwd *pw;
	int printfail;
{
	struct betinfo total;
	int i;

	if (pw->pw_uid < 0) {
		printf("Bad uid %d\n", pw->pw_uid);
		return;
	}
	i = lseek(dbfd, pw->pw_uid * sizeof(struct betinfo), 0);
	if (i < 0) {
		perror("lseek");
		return;
	}
	i = read(dbfd, (char *)&total, sizeof(total));
	if (i < 0) {
		perror("read");
		return;
	}
	if (i == 0 || total.hand == 0) {
		if (printfail)
			printf("%s has never played canfield.\n", pw->pw_name);
		return;
	}
	printf("*----------------------*\n");
	if (total.worth >= 0)
		printf("* Winnings for %-8s*\n", pw->pw_name);
	else
		printf("* Losses for %-10s*\n", pw->pw_name);
	printf("*======================*\n");
	printf("|Costs           Total |\n");
	printf("| Hands       %8d |\n", total.hand);
	printf("| Inspections %8d |\n", total.inspection);
	printf("| Games       %8d |\n", total.game);
	printf("| Runs        %8d |\n", total.runs);
	printf("| Information %8d |\n", total.information);
	printf("| Think time  %8d |\n", total.thinktime);
	printf("|Total Costs  %8d |\n", total.wins - total.worth);
	printf("|Winnings     %8d |\n", total.wins);
	printf("|Net Worth    %8d |\n", total.worth);
	printf("*----------------------*\n\n");
}
