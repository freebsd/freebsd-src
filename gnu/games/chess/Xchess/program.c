
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.1.1.1 $ on $Date: 1993/06/12 14:41:13 $
 *           $Source: /a/cvs/386BSD/src/gnu/chess/Xchess/program.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * The interface to whichever chess playing program we are using...
 */

#include "xchess.h"
#include <signal.h>
#include <sys/time.h>

static int pid;
static FILE *from;
static FILE *to;
static bool easy = 1;

bool
program_init(name)
	char *name;
{
	int toprog[2], fromprog[2];
	char buf[BSIZE];
	char time[10];
	char moves[10];

	pipe(toprog);
	pipe(fromprog);

	if (!(pid = fork())) {
		/* Start up the program. */
		dup2(toprog[0], 0);
		dup2(fromprog[1], 1);
		close(toprog[0]);
		close(toprog[1]);
		close(fromprog[0]);
		close(fromprog[1]);
		sprintf (time, "%d", timeunit/60);
		sprintf (moves, "%d", movesperunit);
		if (proghost)
			execl("/usr/ucb/rsh", "rsh", proghost, name,
					moves, time, 
					(char *) NULL);
		else
			execl(name, name, moves, time, (char *) NULL);
		perror(name);
		exit(1);
	}

	close(toprog[0]);
	close(fromprog[1]);

	from = fdopen(fromprog[0], "r");
	setbuf(from, NULL);
	to = fdopen(toprog[1], "w");
	setbuf(to, NULL);

	/* Get the first line... */
	fgets(buf, BSIZE, from);
	if (debug)
		fprintf(stderr, "program says %s", buf);
	if (blackflag) {
		fputs("switch\n", to);
		fflush(to);
		fgets(buf, BSIZE, from);
		if (debug)
			fprintf(stderr, "program says %s", buf);
		message_add(win1, "GNU Chess playing white\n", false);
	} else
		message_add(win1, "GNU Chess playing black\n", false);

	return (true);
}

void
program_end()
{
	fclose(from);
	fclose(to);
	kill(pid, SIGTERM);
	return;
}

void
program_send(m)
	move *m;
{
	char buf[BSIZE];

	if ((m->type == MOVE) || (m->type == CAPTURE))
		sprintf(buf, "%c%d%c%d\n", 'a' + m->fromx, SIZE - m->fromy,
				'a' + m->tox, SIZE - m->toy);
	else if (m->type == KCASTLE)
		strcpy(buf, (m->piece.color == WHITE) ? "e1g1\n" : "e8g8\n");
	else if (m->type == QCASTLE)
		strcpy(buf, (m->piece.color == WHITE) ? "e1c1\n" : "e8c8\n");

	if (debug)
		fprintf(stderr, "sending program %s", buf);
	if (!easy)
		kill (pid, SIGINT);

	fputs(buf, to);
	fflush(to);

	/* One junk line... */
	fgets(buf, BSIZE, from);
	if (debug)
		fprintf(stderr, "program says %s", buf);
	return;
}

move *
program_get()
{
	int rfd = (1 << fileno(from)), wfd = 0, xfd = 0;
	static struct timeval notime = { 0, 0 };
	char buf[BSIZE], *s;
	move *m;
	int i;

	/* Do a poll... */

#ifdef __386BSD__
	if (!(i = select(32, &rfd, &wfd, &xfd, &notime))) {
#else
	if (!(i = select(32, &rfd, &wfd, &xfd, &notime)) &&
			!from->_cnt) {		/* Bad stuff... */
#endif
		if (debug)
			fprintf(stderr, "poll: nothing\n");
		return (NULL);
	}
	if (i == -1) {
		perror("select");
		return (NULL);
	}

	fgets(buf, BSIZE, from);
	if (*buf == '\n' || *buf == '\0') {
	        message_add(win1, "program died", false);
		return (NULL);
	}

	if (debug)
		fprintf(stderr, "got from program %s", buf);

	for (s = buf; !isalpha(*s); s++)
		;
	m = parse_imove(chessboard, s, nexttomove);
	if (m == NULL)
	 	return (NULL);

	if (!valid_move(m, chessboard)) {
		fprintf(stderr, "Error: move %s is invalid!!\n", buf);
		return (NULL);
	}

	/*
	fgets(buf, BSIZE, from);
	if (debug)
		fprintf(stderr, "program says %s", buf);
	*/
	message_add(win1, buf, false);
	return (m);
}

void
program_undo()
{
	fputs("undo\n", to);
	return;
}
void
program_easy (mode)
	bool mode;

{
	fputs("easy\n", to);
	easy = mode;
}
