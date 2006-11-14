/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 */


#include <sys/devicestat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <curses.h>
#include <devstat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <histedit.h>
#include <libgeom.h>
#include <paths.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static int flag_a, flag_c, flag_d;
static int flag_I = 500000;

static void usage(void);

static const char*
el_prompt(void)
{

	return ("Filter: ");
}

int
main(int argc, char **argv)
{
	int error, i, quit;
	int curx, cury, maxx, maxy, line_len, max_flen;
	struct devstat *gsp, *gsq;
	void *sp, *sq;
	double dt;
	struct timespec tp, tq;
	struct gmesh gmp;
	struct gprovider *pp;
	struct gconsumer *cp;
	struct gident *gid;
	regex_t f_re, tmp_f_re;
	short cf, cb;
	char *p;
	char f_s[100], pf_s[100], tmp_f_s[100];
	const char *line;
	long double ld[11];
	uint64_t u64;
	EditLine *el;
	History *hist;
	HistEvent hist_ev;

	f_s[0] = '\0';
	while ((i = getopt(argc, argv, "adcf:I:")) != -1) {
		switch (i) {
		case 'a':
			flag_a = 1;
			break;
		case 'c':
			flag_c = 1;
			break;
		case 'd':
			flag_d = 1;
			break;
		case 'f':
			if (strlen(optarg) > sizeof(f_s) - 1)
				errx(EX_USAGE, "Filter string too long");
			if (regcomp(&f_re, optarg, REG_EXTENDED) != 0)
				errx(EX_USAGE,
				    "Invalid filter - see re_format(7)");
			strncpy(f_s, optarg, sizeof(f_s));
			break;
		case 'I':
			p = NULL;
			i = strtoul(optarg, &p, 0);
			if (p == optarg || errno == EINVAL ||
			    errno == ERANGE) {
				errx(1, "Invalid argument to -I");
			} else if (!strcmp(p, "s"))
				i *= 1000000;
			else if (!strcmp(p, "ms"))
				i *= 1000;
			else if (!strcmp(p, "us"))
				i *= 1;
			flag_I = i;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	i = geom_gettree(&gmp);
	if (i != 0)
		err(1, "geom_gettree = %d", i);
	error = geom_stats_open();
	if (error)
		err(1, "geom_stats_open()");
	sq = NULL;
	sq = geom_stats_snapshot_get();
	if (sq == NULL)
		err(1, "geom_stats_snapshot()");
	/* Setup curses */
	initscr();
	start_color();
	use_default_colors();
	pair_content(0, &cf, &cb);
	init_pair(1, COLOR_GREEN, cb);
	init_pair(2, COLOR_MAGENTA, cb);
	init_pair(3, COLOR_RED, cb);
	cbreak();
	noecho();
	nonl();
	nodelay(stdscr, 1);
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	/* Setup libedit */
	hist = history_init();
	if (hist == NULL)
		err(EX_SOFTWARE, "history_init()");
	history(hist, &hist_ev, H_SETSIZE, 100);
	el = el_init("gstat", stdin, stdout, stderr);
	if (el == NULL)
		err(EX_SOFTWARE, "el_init");
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_SIGNAL, 1);
	el_set(el, EL_HIST, history, hist);
	el_set(el, EL_PROMPT, el_prompt);
	if (f_s[0] != '\0')
		history(hist, &hist_ev, H_ENTER, f_s);
	geom_stats_snapshot_timestamp(sq, &tq);
	for (quit = 0; !quit;) {
		sp = geom_stats_snapshot_get();
		if (sp == NULL)
			err(1, "geom_stats_snapshot()");
		geom_stats_snapshot_timestamp(sp, &tp);
		dt = tp.tv_sec - tq.tv_sec;
		dt += (tp.tv_nsec - tq.tv_nsec) * 1e-9;
		tq = tp;
	
		geom_stats_snapshot_reset(sp);
		geom_stats_snapshot_reset(sq);
		move(0,0);
		printw("dT: %5.3fs  w: %.3fs",
		    dt, (float)flag_I / 1000000);
		if (f_s[0] != '\0') {
			printw("  filter: ");
			getyx(stdscr, cury, curx);
			getmaxyx(stdscr, maxy, maxx);
			strncpy(pf_s, f_s, sizeof(pf_s));
			max_flen = maxx - curx - 1;
			if ((int)strlen(f_s) > max_flen && max_flen >= 0) {
				if (max_flen > 3)
					pf_s[max_flen - 3] = '.';
				if (max_flen > 2)
					pf_s[max_flen - 2] = '.';
				if (max_flen > 1)
					pf_s[max_flen - 1] = '.';
				pf_s[max_flen] = '\0';
			}
			printw("%s", pf_s);
		}
		printw("\n");
		printw(" L(q)  ops/s   ");
		printw(" r/s   kBps   ms/r   ");
		printw(" w/s   kBps   ms/w   ");
		if (flag_d)
			printw(" d/s   kBps   ms/d   ");
		printw("%%busy Name\n");
		for (;;) {
			gsp = geom_stats_snapshot_next(sp);
			gsq = geom_stats_snapshot_next(sq);
			if (gsp == NULL || gsq == NULL)
				break;
			if (gsp->id == NULL)
				continue;
			gid = geom_lookupid(&gmp, gsp->id);
			if (gid == NULL) {
				geom_deletetree(&gmp);
				i = geom_gettree(&gmp);
				if (i != 0)
					err(1, "geom_gettree = %d", i);
				gid = geom_lookupid(&gmp, gsp->id);
			}
			if (gid == NULL)
				continue;
			if (gid->lg_what == ISCONSUMER && !flag_c)
				continue;
			/* Do not print past end of window */
			getyx(stdscr, cury, curx);
			if (curx > 0)
				continue;
			if ((gid->lg_what == ISPROVIDER
			    || gid->lg_what == ISCONSUMER) && f_s[0] != '\0') {
				pp = gid->lg_ptr;
				if ((regexec(&f_re, pp->lg_name, 0, NULL, 0)
				     != 0))
				  continue;
			}
			if (gsp->sequence0 != gsp->sequence1) {
				printw("*\n");
				continue;
			}
			devstat_compute_statistics(gsp, gsq, dt, 
			    DSM_QUEUE_LENGTH, &u64,
			    DSM_TRANSFERS_PER_SECOND, &ld[0],

			    DSM_TRANSFERS_PER_SECOND_READ, &ld[1],
			    DSM_MB_PER_SECOND_READ, &ld[2],
			    DSM_MS_PER_TRANSACTION_READ, &ld[3],

			    DSM_TRANSFERS_PER_SECOND_WRITE, &ld[4],
			    DSM_MB_PER_SECOND_WRITE, &ld[5],
			    DSM_MS_PER_TRANSACTION_WRITE, &ld[6],

			    DSM_BUSY_PCT, &ld[7],
			    DSM_TRANSFERS_PER_SECOND_FREE, &ld[8],
			    DSM_MB_PER_SECOND_FREE, &ld[9],
			    DSM_MS_PER_TRANSACTION_FREE, &ld[10],
			    DSM_NONE);

			if (flag_a && ld[7] < 0.1) {
				*gsq = *gsp;
				continue;
			}

			printw(" %4ju", (uintmax_t)u64);
			printw(" %6.0f", (double)ld[0]);
			printw(" %6.0f", (double)ld[1]);
			printw(" %6.0f", (double)ld[2] * 1024);
			printw(" %6.1f", (double)ld[3]);
			printw(" %6.0f", (double)ld[4]);
			printw(" %6.0f", (double)ld[5] * 1024);
			printw(" %6.1f", (double)ld[6]);

			if (flag_d) {
				printw(" %6.0f", (double)ld[8]);
				printw(" %6.0f", (double)ld[9] * 1024);
				printw(" %6.1f", (double)ld[10]);
			}

			if (ld[7] > 80)
				i = 3;
			else if (ld[7] > 50)
				i = 2;
			else 
				i = 1;
			attron(COLOR_PAIR(i));
			printw(" %6.1lf", (double)ld[7]);
			attroff(COLOR_PAIR(i));
			printw("|");
			if (gid == NULL) {
				printw(" ??");
			} else if (gid->lg_what == ISPROVIDER) {
				pp = gid->lg_ptr;
				printw(" %s", pp->lg_name);
			} else if (gid->lg_what == ISCONSUMER) {
				cp = gid->lg_ptr;
				printw(" %s/%s/%s",
				    cp->lg_geom->lg_class->lg_name,
				    cp->lg_geom->lg_name,
				    cp->lg_provider->lg_name);
			}
			clrtoeol();
			printw("\n");
			*gsq = *gsp;
		}
		geom_stats_snapshot_free(sp);
		getyx(stdscr, cury, curx);
		getmaxyx(stdscr, maxy, maxx);
		clrtobot();
		if (maxy - 1 <= cury)
			move(maxy - 1, 0);
		refresh();
		usleep(flag_I);
		while((i = getch()) != ERR) {
			switch (i) {
			case '>':
				flag_I *= 2;
				break;
			case '<':
				flag_I /= 2;
				if (flag_I < 1000)
					flag_I = 1000;
				break;
			case 'c':
				flag_c = !flag_c;
				break;
			case 'f':
				move(0,0);
				clrtoeol();
				refresh();
				line = el_gets(el, &line_len);
				if (line == NULL)
					err(1, "el_gets");
				if (line_len > 1)
					history(hist, &hist_ev, H_ENTER, line);
				strncpy(tmp_f_s, line, sizeof(f_s));
				if ((p = strchr(tmp_f_s, '\n')) != NULL)
					*p = '\0';
				/*
				 * We have to clear since we messed up
				 * curses idea of the screen by using
				 * libedit.
				 */
				clear();
				refresh();
				if (regcomp(&tmp_f_re, tmp_f_s, REG_EXTENDED)
				    != 0) {
					move(0, 0);
					printw("Invalid filter");
					refresh();
					sleep(1);
				} else {
					strncpy(f_s, tmp_f_s, sizeof(f_s));
					f_re = tmp_f_re;
				}
				break;
			case 'F':
				f_s[0] = '\0';
				break;
			case 'q':
				quit = 1;
				break;
			default:
				break;
			}
		}
	}

	endwin();
	el_end(el);
	exit(EX_OK);
}

static void
usage(void)
{
	fprintf(stderr, "usage: gstat [-acd] [-f filter] [-I interval]\n");
	exit(EX_USAGE);
        /* NOTREACHED */
}
