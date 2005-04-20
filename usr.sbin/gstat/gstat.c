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


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <curses.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <libgeom.h>
#include <sys/resource.h>
#include <devstat.h>
#include <sys/devicestat.h>

static int flag_c, flag_d;
static int flag_I = 500000;

static void usage(void);

int
main(int argc, char **argv)
{
	int error, i, quit;
	struct devstat *gsp, *gsq;
	void *sp, *sq;
	double dt;
	struct timespec tp, tq;
	struct gmesh gmp;
	struct gprovider *pp;
	struct gconsumer *cp;
	struct gident *gid;
	short cf, cb;
	char *p;
	long double ld[11];
	uint64_t u64;

	while ((i = getopt(argc, argv, "dcI:")) != -1) {
		switch (i) {
		case 'c':
			flag_c = 1;
			break;
		case 'd':
			flag_d = 1;
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
		printw("dT: %5.3f  flag_I %dus  sizeof %d  i %d\n",
		    dt, flag_I, sizeof(*gsp), i);
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
			if (gid != NULL && gid->lg_what == ISCONSUMER &&
			    !flag_c)
				continue;
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
		clrtobot();
		refresh();
		usleep(flag_I);
		i = getch();
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
		case 'q':
			quit = 1;
			break;
		default:
			break;
		}
	}

	endwin();
	exit (0);
}

static void
usage(void)
{
        fprintf(stderr, "usage: gstat [-cd] [-I interval]\n");
        exit(1);
        /* NOTREACHED */
}
