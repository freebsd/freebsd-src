/*
 * Copyright (c) 1980, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)snscore.c	8.1 (Berkeley) 7/19/93";
#endif /* not lint */

#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include "pathnames.h"

char *recfile = _PATH_RAWSCORES;
#define MAXPLAYERS 256

struct	player	{
	short	uids;
	short	scores;
	char	*name;
} players[MAXPLAYERS], temp;

int
main()
{
	short	uid, score;
	FILE	*fd;
	int	noplayers;
	int	i, j, notsorted;
	short	whoallbest, allbest;
	char	*q;
	struct	passwd	*p;

	fd = fopen(recfile, "r");

	if (fd == NULL) {
		perror(recfile);
		exit(1);
	}

	/* revoke privs */
	setegid(getgid());
	setgid(getgid());

	printf("Snake players scores to date\n");
	fread(&whoallbest, sizeof(short), 1, fd);
	fread(&allbest, sizeof(short), 1, fd);
	noplayers = 0;
	for (uid = 2; ;uid++) {
		if(fread(&score, sizeof(short), 1, fd) == 0)
			break;
		if (score > 0) {
			if (noplayers > MAXPLAYERS) {
				printf("too many players\n");
				exit(2);
			}
			players[noplayers].uids = uid;
			players[noplayers].scores = score;
			p = getpwuid(uid);
			if (p == NULL)
				continue;
			q = p -> pw_name;
			players[noplayers].name = malloc(strlen(q) + 1);
			strcpy(players[noplayers].name, q);
			noplayers++;
		}
	}

	/* bubble sort scores */
	for (notsorted = 1; notsorted; ) {
		notsorted = 0;
		for (i = 0; i < noplayers - 1; i++)
			if (players[i].scores < players[i + 1].scores) {
				temp = players[i];
				players[i] = players[i + 1];
				players[i + 1] = temp;
				notsorted++;
			}
	}

	j = 1;
	for (i = 0; i < noplayers; i++) {
		printf("%d:\t$%d\t%s\n", j, players[i].scores, players[i].name);
		if (players[i].scores > players[i + 1].scores)
			j = i + 2;
	}
	exit(0);
}
