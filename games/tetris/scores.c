/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
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
 *
 *	@(#)scores.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Score code for Tetris, by Darren Provine (kilroy@gboro.glassboro.edu)
 * modified 22 January 1992, to limit the number of entries any one
 * person has.
 *
 * Major whacks since then.
 */
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * XXX - need a <termcap.h>
 */
int	tputs __P((const char *, int, int (*)(int)));

#include "pathnames.h"
#include "screen.h"
#include "scores.h"
#include "tetris.h"

/*
 * Within this code, we can hang onto one extra "high score", leaving
 * room for our current score (whether or not it is high).
 *
 * We also sometimes keep tabs on the "highest" score on each level.
 * As long as the scores are kept sorted, this is simply the first one at
 * that level.
 */
#define NUMSPOTS (MAXHISCORES + 1)
#define	NLEVELS (MAXLEVEL + 1)

static time_t now;
static int nscores;
static int gotscores;
static struct highscore scores[NUMSPOTS];

static int checkscores __P((struct highscore *, int));
static int cmpscores __P((const void *, const void *));
static void getscores __P((FILE **));
static void printem __P((int, int, struct highscore *, int, const char *));
static char *thisuser __P((void));

/*
 * Read the score file.  Can be called from savescore (before showscores)
 * or showscores (if savescore will not be called).  If the given pointer
 * is not NULL, sets *fpp to an open file pointer that corresponds to a
 * read/write score file that is locked with LOCK_EX.  Otherwise, the
 * file is locked with LOCK_SH for the read and closed before return.
 *
 * Note, we assume closing the stdio file releases the lock.
 */
static void
getscores(fpp)
	FILE **fpp;
{
	int sd, mint, lck;
	char *mstr, *human;
	FILE *sf;

	if (fpp != NULL) {
		mint = O_RDWR | O_CREAT;
		mstr = "r+";
		human = "read/write";
		lck = LOCK_EX;
	} else {
		mint = O_RDONLY;
		mstr = "r";
		human = "reading";
		lck = LOCK_SH;
	}
	sd = open(_PATH_SCOREFILE, mint, 0666);
	if (sd < 0) {
		if (fpp == NULL) {
			nscores = 0;
			return;
		}
		(void)fprintf(stderr, "tetris: cannot open %s for %s: %s\n",
		    _PATH_SCOREFILE, human, strerror(errno));
		exit(1);
	}
	if ((sf = fdopen(sd, mstr)) == NULL) {
		(void)fprintf(stderr, "tetris: cannot fdopen %s for %s: %s\n",
		    _PATH_SCOREFILE, human, strerror(errno));
		exit(1);
	}

	/*
	 * Grab a lock.
	 */
	if (flock(sd, lck))
		(void)fprintf(stderr,
		    "tetris: warning: score file %s cannot be locked: %s\n",
		    _PATH_SCOREFILE, strerror(errno));

	nscores = fread(scores, sizeof(scores[0]), MAXHISCORES, sf);
	if (ferror(sf)) {
		(void)fprintf(stderr, "tetris: error reading %s: %s\n",
		    _PATH_SCOREFILE, strerror(errno));
		exit(1);
	}

	if (fpp)
		*fpp = sf;
	else
		(void)fclose(sf);
}

void
savescore(level)
	int level;
{
	register struct highscore *sp;
	register int i;
	int change;
	FILE *sf;
	const char *me;

	getscores(&sf);
	gotscores = 1;
	(void)time(&now);

	/*
	 * Allow at most one score per person per level -- see if we
	 * can replace an existing score, or (easiest) do nothing.
	 * Otherwise add new score at end (there is always room).
	 */
	change = 0;
	me = thisuser();
	for (i = 0, sp = &scores[0]; i < nscores; i++, sp++) {
		if (sp->hs_level != level || strcmp(sp->hs_name, me) != 0)
			continue;
		if (score > sp->hs_score) {
			(void)printf("%s bettered %s %d score of %d!\n",
			    "\nYou", "your old level", level,
			    sp->hs_score * sp->hs_level);
			sp->hs_score = score;	/* new score */
			sp->hs_time = now;	/* and time */
			change = 1;
		} else if (score == sp->hs_score) {
			(void)printf("%s tied %s %d high score.\n",
			    "\nYou", "your old level", level);
			sp->hs_time = now;	/* renew it */
			change = 1;		/* gotta rewrite, sigh */
		} /* else new score < old score: do nothing */
		break;
	}
	if (i >= nscores) {
		strcpy(sp->hs_name, me);
		sp->hs_level = level;
		sp->hs_score = score;
		sp->hs_time = now;
		nscores++;
		change = 1;
	}

	if (change) {
		/*
		 * Sort & clean the scores, then rewrite.
		 */
		nscores = checkscores(scores, nscores);
		rewind(sf);
		if (fwrite(scores, sizeof(*sp), nscores, sf) != nscores ||
		    fflush(sf) == EOF)
			(void)fprintf(stderr,
			    "tetris: error writing %s: %s -- %s\n",
			    _PATH_SCOREFILE, strerror(errno),
			    "high scores may be damaged");
	}
	(void)fclose(sf);	/* releases lock */
}

/*
 * Get login name, or if that fails, get something suitable.
 * The result is always trimmed to fit in a score.
 */
static char *
thisuser()
{
	register const char *p;
	register struct passwd *pw;
	register size_t l;
	static char u[sizeof(scores[0].hs_name)];

	if (u[0])
		return (u);
	p = getlogin();
	if (p == NULL || *p == '\0') {
		pw = getpwuid(getuid());
		if (pw != NULL)
			p = pw->pw_name;
		else
			p = "  ???";
	}
	l = strlen(p);
	if (l >= sizeof(u))
		l = sizeof(u) - 1;
	bcopy(p, u, l);
	u[l] = '\0';
	return (u);
}

/*
 * Score comparison function for qsort.
 *
 * If two scores are equal, the person who had the score first is
 * listed first in the highscore file.
 */
static int
cmpscores(x, y)
	const void *x, *y;
{
	register const struct highscore *a, *b;
	register long l;

	a = x;
	b = y;
	l = (long)b->hs_level * b->hs_score - (long)a->hs_level * a->hs_score;
	if (l < 0)
		return (-1);
	if (l > 0)
		return (1);
	if (a->hs_time < b->hs_time)
		return (-1);
	if (a->hs_time > b->hs_time)
		return (1);
	return (0);
}

/*
 * If we've added a score to the file, we need to check the file and ensure
 * that this player has only a few entries.  The number of entries is
 * controlled by MAXSCORES, and is to ensure that the highscore file is not
 * monopolised by just a few people.  People who no longer have accounts are
 * only allowed the highest score.  Scores older than EXPIRATION seconds are
 * removed, unless they are someone's personal best.
 * Caveat:  the highest score on each level is always kept.
 */
static int
checkscores(hs, num)
	register struct highscore *hs;
	int num;
{
	register struct highscore *sp;
	register int i, j, k, numnames;
	int levelfound[NLEVELS];
	struct peruser {
		char *name;
		int times;
	} count[NUMSPOTS];
	register struct peruser *pu;

	/*
	 * Sort so that highest totals come first.
	 *
	 * levelfound[i] becomes set when the first high score for that
	 * level is encountered.  By definition this is the highest score.
	 */
	qsort((void *)hs, nscores, sizeof(*hs), cmpscores);
	for (i = MINLEVEL; i < NLEVELS; i++)
		levelfound[i] = 0;
	numnames = 0;
	for (i = 0, sp = hs; i < num;) {
		/*
		 * This is O(n^2), but do you think we care?
		 */
		for (j = 0, pu = count; j < numnames; j++, pu++)
			if (strcmp(sp->hs_name, pu->name) == 0)
				break;
		if (j == numnames) {
			/*
			 * Add new user, set per-user count to 1.
			 */
			pu->name = sp->hs_name;
			pu->times = 1;
			numnames++;
		} else {
			/*
			 * Two ways to keep this score:
			 * - Not too many (per user), still has acct, &
			 *	score not dated; or
			 * - High score on this level.
			 */
			if ((pu->times < MAXSCORES &&
			     getpwnam(sp->hs_name) != NULL &&
			     sp->hs_time + EXPIRATION >= now) ||
			    levelfound[sp->hs_level] == 0)
				pu->times++;
			else {
				/*
				 * Delete this score, do not count it,
				 * do not pass go, do not collect $200.
				 */
				num--;
				for (k = i; k < num; k++)
					hs[k] = hs[k + 1];
				continue;
			}
		}
		levelfound[sp->hs_level] = 1;
		i++, sp++;
	}
	return (num > MAXHISCORES ? MAXHISCORES : num);
}

/*
 * Show current scores.  This must be called after savescore, if
 * savescore is called at all, for two reasons:
 * - Showscores munches the time field.
 * - Even if that were not the case, a new score must be recorded
 *   before it can be shown anyway.
 */
void
showscores(level)
	int level;
{
	register struct highscore *sp;
	register int i, n, c;
	const char *me;
	int levelfound[NLEVELS];

	if (!gotscores)
		getscores((FILE **)NULL);
	(void)printf("\n\t\t\t    Tetris High Scores\n");

	/*
	 * If level == 0, the person has not played a game but just asked for
	 * the high scores; we do not need to check for printing in highlight
	 * mode.  If SOstr is null, we can't do highlighting anyway.
	 */
	me = level && SOstr ? thisuser() : NULL;

	/*
	 * Set times to 0 except for high score on each level.
	 */
	for (i = MINLEVEL; i < NLEVELS; i++)
		levelfound[i] = 0;
	for (i = 0, sp = scores; i < nscores; i++, sp++) {
		if (levelfound[sp->hs_level])
			sp->hs_time = 0;
		else {
			sp->hs_time = 1;
			levelfound[sp->hs_level] = 1;
		}
	}

	/*
	 * Page each screenful of scores.
	 */
	for (i = 0, sp = scores; i < nscores; sp += n) {
		n = 40;
		if (i + n > nscores)
			n = nscores - i;
		printem(level, i + 1, sp, n, me);
		if ((i += n) < nscores) {
			(void)printf("\nHit RETURN to continue.");
			(void)fflush(stdout);
			while ((c = getchar()) != '\n')
				if (c == EOF)
					break;
			(void)printf("\n");
		}
	}
}

static void
printem(level, offset, hs, n, me)
	int level, offset;
	register struct highscore *hs;
	register int n;
	const char *me;
{
	register struct highscore *sp;
	int nrows, row, col, item, i, highlight;
	char buf[100];
#define	TITLE "Rank  Score   Name     (points/level)"

	/*
	 * This makes a nice two-column sort with headers, but it's a bit
	 * convoluted...
	 */
	printf("%s   %s\n", TITLE, n > 1 ? TITLE : "");

	highlight = 0;
	nrows = (n + 1) / 2;

	for (row = 0; row < nrows; row++) {
		for (col = 0; col < 2; col++) {
			item = col * nrows + row;
			if (item >= n) {
				/*
				 * Can only occur on trailing columns.
				 */
				(void)putchar('\n');
				continue;
			}
			(void)printf(item + offset < 10 ? "  " : " ");
			sp = &hs[item];
			(void)sprintf(buf,
			    "%d%c %6d  %-11s (%d on %d)",
			    item + offset, sp->hs_time ? '*' : ' ',
			    sp->hs_score * sp->hs_level,
			    sp->hs_name, sp->hs_score, sp->hs_level);
			/*
			 * Highlight if appropriate.  This works because
			 * we only get one score per level.
			 */
			if (me != NULL &&
			    sp->hs_level == level &&
			    sp->hs_score == score &&
			    strcmp(sp->hs_name, me) == 0) {
				putpad(SOstr);
				highlight = 1;
			}
			(void)printf("%s", buf);
			if (highlight) {
				putpad(SEstr);
				highlight = 0;
			}

			/* fill in spaces so column 1 lines up */
			if (col == 0)
				for (i = 40 - strlen(buf); --i >= 0;)
					(void)putchar(' ');
			else /* col == 1 */
				(void)putchar('\n');
		}
	}
}
