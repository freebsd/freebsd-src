/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ed James.
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

/*
 * Copyright (c) 1987 by Ed James, UC Berkeley.  All rights reserved.
 *
 * Copy permission is hereby granted provided that this notice is
 * retained on all partial or complete copies.
 *
 * For more info on this and all of my stuff, mail edjames@berkeley.edu.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)log.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif not lint

#include <string.h>
#include "include.h"
#include "pathnames.h"

#ifdef SYSV
#include <sys/utsname.h>
#endif

static FILE *score_fp;

int
compar(va, vb)
	const void *va, *vb;
{
	const SCORE	*a, *b;

	a = (const SCORE *)va;
	b = (const SCORE *)vb;

	if (b->planes == a->planes)
		return (b->time - a->time);
	else
		return (b->planes - a->planes);
}

#define SECAMIN		60
#define MINAHOUR	60
#define HOURADAY	24
#define SECAHOUR	(SECAMIN * MINAHOUR)
#define SECADAY		(SECAHOUR * HOURADAY)
#define DAY(t)		((t) / SECADAY)
#define HOUR(t)		(((t) % SECADAY) / SECAHOUR)
#define MIN(t)		(((t) % SECAHOUR) / SECAMIN)
#define SEC(t)		((t) % SECAMIN)

const char	*
timestr(t)
{
	static char	s[80];

	if (DAY(t) > 0)
		(void)sprintf(s, "%dd+%02dhrs", DAY(t), HOUR(t));
	else if (HOUR(t) > 0)
		(void)sprintf(s, "%d:%02d:%02d", HOUR(t), MIN(t), SEC(t));
	else if (MIN(t) > 0)
		(void)sprintf(s, "%d:%02d", MIN(t), SEC(t));
	else if (SEC(t) > 0)
		(void)sprintf(s, ":%02d", SEC(t));
	else
		*s = '\0';

	return (s);
}

void
open_score_file()
{
	mode_t old_mask;
	int score_fd;
	int flags;

	old_mask = umask(0);
	score_fd = open(_PATH_SCORE, O_CREAT|O_RDWR, 0664);
	umask(old_mask);
	if (score_fd < 0) {
		warn("open %s", _PATH_SCORE);
		return;
	}
	/* Set the close-on-exec flag.  If this fails for any reason, quit
	 * rather than leave the score file open to tampering.  */
	flags = fcntl(score_fd, F_GETFD);
	if (flags < 0)
		err(1, "fcntl F_GETFD");
	flags |= FD_CLOEXEC;
	if (fcntl(score_fd, F_SETFD, flags) == -1)
		err(1, "fcntl F_SETFD");
	/*
	 * This is done to take advantage of stdio, while still
	 * allowing a O_CREAT during the open(2) of the log file.
	 */
	score_fp = fdopen(score_fd, "r+");
	if (score_fp == NULL) {
		warn("fdopen %s", _PATH_SCORE);
		return;
	}
}

int
log_score(list_em)
	int list_em;
{
	int		i, num_scores = 0, good, changed = 0, found = 0;
	struct passwd	*pw;
	char		*cp;
	SCORE		score[100], thisscore;
#ifdef SYSV
	struct utsname	name;
#endif

	if (score_fp == NULL) {
		warnx("no score file available");
		return (-1);
	}

#ifdef BSD
	if (flock(fileno(score_fp), LOCK_EX) < 0)
#endif
#ifdef SYSV
	while (lockf(fileno(score_fp), F_LOCK, 1) < 0)
#endif
	{
		warn("flock %s", _PATH_SCORE);
		return (-1);
	}
	for (;;) {
		good = fscanf(score_fp, SCORE_SCANF_FMT,
			score[num_scores].name,
			score[num_scores].host,
			score[num_scores].game,
			&score[num_scores].planes,
			&score[num_scores].time,
			&score[num_scores].real_time);
		if (good != 6 || ++num_scores >= NUM_SCORES)
			break;
	}
	if (!test_mode && !list_em) {
		if ((pw = (struct passwd *) getpwuid(getuid())) == NULL) {
			fprintf(stderr,
				"getpwuid failed for uid %d.  Who are you?\n",
				(int)getuid());
			return (-1);
		}
		strcpy(thisscore.name, pw->pw_name);
#ifdef BSD
		if (gethostname(thisscore.host, sizeof (thisscore.host)) < 0) {
			perror("gethostname");
			return (-1);
		}
#endif
#ifdef SYSV
		uname(&name);
		strcpy(thisscore.host, name.nodename);
#endif

		cp = rindex(file, '/');
		if (cp == NULL) {
			fprintf(stderr, "log: where's the '/' in %s?\n", file);
			return (-1);
		}
		cp++;
		strcpy(thisscore.game, cp);

		thisscore.time = clck;
		thisscore.planes = safe_planes;
		thisscore.real_time = time(0) - start_time;

		for (i = 0; i < num_scores; i++) {
			if (strcmp(thisscore.name, score[i].name) == 0 &&
			    strcmp(thisscore.host, score[i].host) == 0 &&
			    strcmp(thisscore.game, score[i].game) == 0) {
				if (thisscore.time > score[i].time) {
					score[i].time = thisscore.time;
					score[i].planes = thisscore.planes;
					score[i].real_time =
						thisscore.real_time;
					changed++;
				}
				found++;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < num_scores; i++) {
				if (thisscore.time > score[i].time) {
					if (num_scores < NUM_SCORES)
						num_scores++;
					bcopy(&score[i],
						&score[num_scores - 1],
						sizeof (score[i]));
					bcopy(&thisscore, &score[i],
						sizeof (score[i]));
					changed++;
					break;
				}
			}
		}
		if (!found && !changed && num_scores < NUM_SCORES) {
			bcopy(&thisscore, &score[num_scores],
				sizeof (score[num_scores]));
			num_scores++;
			changed++;
		}

		if (changed) {
			if (found)
				puts("You beat your previous score!");
			else
				puts("You made the top players list!");
			qsort(score, num_scores, sizeof (*score), compar);
			rewind(score_fp);
			for (i = 0; i < num_scores; i++)
				fprintf(score_fp, "%s %s %s %d %d %d\n",
					score[i].name, score[i].host,
					score[i].game, score[i].planes,
					score[i].time, score[i].real_time);
		fflush(score_fp);
		if (ferror(score_fp))
			warn("error writing %s", _PATH_SCORE);
		/* It is just possible that updating an entry could
		 * have reduced the length of the file, so we
		 * truncate it.  The lseek is required for stream/fd
		 * synchronisation by POSIX.1.  */
		lseek(fileno(score_fp), 0, SEEK_END);
		ftruncate(fileno(score_fp), ftell(score_fp));
		} else {
			if (found)
				puts("You didn't beat your previous score.");
			else
				puts("You didn't make the top players list.");
		}
		putchar('\n');
	}
#ifdef BSD
	flock(fileno(score_fp), LOCK_UN);
#endif
#ifdef SYSV
	/* lock will evaporate upon close */
#endif
	fclose(score_fp);
	printf("%2s:  %-8s  %-8s  %-18s  %4s  %9s  %4s\n", "#", "name", "host",
		"game", "time", "real time", "planes safe");
	puts("-------------------------------------------------------------------------------");
	for (i = 0; i < num_scores; i++) {
		cp = index(score[i].host, '.');
		if (cp != NULL)
			*cp = '\0';
		printf("%2d:  %-8s  %-8s  %-18s  %4d  %9s  %4d\n", i + 1,
			score[i].name, score[i].host, score[i].game,
			score[i].time, timestr(score[i].real_time),
			score[i].planes);
	}
	putchar('\n');
	return (0);
}
