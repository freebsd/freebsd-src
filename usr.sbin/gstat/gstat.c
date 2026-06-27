/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 */


#include <sys/devicestat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <assert.h>
#include <curses.h>
#include <devstat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <histedit.h>
#include <libgeom.h>
#include <paths.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define HIGH_PCT_BUSY_THRESH 80
#define MEDIUM_PCT_BUSY_THRESH 50

static int flag_a, flag_b, flag_B, flag_c, flag_C, flag_d, flag_o, flag_p,
	   flag_s;
static int flag_I = 1000000;

static int quit;
static int max_flen;
static History *hist;
static EditLine *el;
static HistEvent hist_ev;
static regex_t f_re;
static char pf_s[100];
static char f_s[100];
static uintmax_t loop;
static double dt;
static double byte_scale = 1024;
static const char *byte_unit = "k";

struct kstat {
	long double		transfers_per_second;
	long double		mb_per_second;
	long double		ms_per_transaction;
	long double		kb_per_transfer;
};

static void __dead2
usage(void)
{
	fprintf(stderr,
		"usage: gstat [-abBcCdgkmops] [-f filter] [-I interval]\n");
	exit(EX_USAGE);
	/* NOTREACHED */
}

static void
use_kb(void)
{
	byte_scale = 1024;
	byte_unit = "k";
}

static void
use_mb(void)
{
	byte_scale = 1;
	byte_unit = "M";
}

static void
use_gb(void)
{
	byte_scale = 1.0/1024;
	byte_unit = "G";
}

static void
printer(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (flag_B || (flag_b && loop == 2))
		vprintf(fmt, ap);
	if (!flag_b)
		vw_printw(stdscr, fmt, ap);
	va_end(ap);
}

static void
text_fld_header(const char *k)
{
	if (flag_s)
		printer(" %s/s     kB   %sBps   ms/%s   ", k, byte_unit, k);
	else
		printer(" %s/s   %sBps   ms/%s   ", k, byte_unit, k);
}

static void
text_header(void)
{
	int curx = 0, cury = 0;
	int maxx = 0, maxy = 0;

	if (!flag_b)
		move(0,0);
	printer("dT: %5.3fs  w: %.3fs", dt,
			(float)flag_I / 1000000);
	if (f_s[0] != '\0') {
		printer("  filter: ");
		if (!flag_b) {
			getyx(stdscr, cury, curx);
			(void)cury;
			getmaxyx(stdscr, maxy, maxx);
			(void)maxy;
		}
		strlcpy(pf_s, f_s, sizeof(pf_s));
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
		printer("%s", pf_s);
	}

	if (!flag_b)
		clrtoeol();
	printer("\n");

	printer(" L(q)  ops/s   ");
	text_fld_header("r");
	text_fld_header("w");
	if (flag_d)
		text_fld_header("d");
	if (flag_o)
		printer(" o/s   ms/o   ");
	printer("%%busy Name");
	if (!flag_b)
		clrtoeol();
	printer("\n");
}

static void
print_kstat_text(const struct kstat *ks, int other)
{
	printer(" %6.0f", (double)ks->transfers_per_second);
	if (flag_s && !other)
		printer(" %6.0f", (double)ks->kb_per_transfer);
	if (!other)
		printer(" %6.0f", (double)ks->mb_per_second * byte_scale);
	if (ks->ms_per_transaction < 100)
		printer(" %6.3f", (double)ks->ms_per_transaction);
	else if (ks->ms_per_transaction < 1000)
		printer(" %6.2f", (double)ks->ms_per_transaction);
	else if (ks->ms_per_transaction < 10000)
		printer(" %6.1f", (double)ks->ms_per_transaction);
	else
		printer(" %6.0f", (double)ks->ms_per_transaction);
}

static void
print_head_csv(void)
{

	printf("timestamp,name,q-depth,total_ops/s");
	if (flag_s) {
		printf(",read/s,read_sz-KiB");
		printf(",read-KiB/s,ms/read");
		printf(",write/s,write_sz-KiB");
		printf(",write-KiB/s,ms/write");
	} else {
		printf(",read/s,read-KiB/s,ms/read");
		printf(",write/s,write-KiB/s,ms/write");
	}
	if (flag_d) {
		if (flag_s) {
			printf(",delete/s,delete-sz-KiB");
			printf(",delete-KiB/s,ms/delete");
		} else {
			printf(",delete/s,delete-KiB/s");
			printf(",ms/delete");
		}
	}
	if (flag_o)
		printf(",other/s,ms/other");
	printf(",%%busy\n");
}

static void
print_kstat_csv(const struct kstat *ks, int other)
{

	printf(",%.0f", (double)ks->transfers_per_second);
	if (flag_s && !other)
		printf(",%.0f", (double)ks->kb_per_transfer);
	if (!other)
		printf(",%.0f", (double)ks->mb_per_second * 1024);
	printf(",%.3f", (double)ks->ms_per_transaction);
}

static const char*
el_prompt(void)
{

	return ("Filter: ");
}

static void
setup_interactive(void)
{
	short cf, cb;

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
}

static int
poll_input(void)
{
	int i, line_len;
	char *p;
	const char *line;
	char tmp_f_s[100];
	regex_t tmp_f_re;

	i = getch();
	if (i == ERR)
		return (0);
	switch (i) {
	case '>':
		flag_I *= 2;
		break;
	case '<':
		flag_I /= 2;
		if (flag_I < 1000)
			flag_I = 1000;
		break;
	case '1':
		flag_I = 1000000;
		break;
	case 'c':
		flag_c = !flag_c;
		break;
	case 'd':
		flag_d = !flag_d;
		break;
	case 'o':
		flag_o = !flag_o;
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
		strlcpy(tmp_f_s, line, sizeof(f_s));
		if ((p = strchr(tmp_f_s, '\n')) != NULL)
			*p = '\0';
		/*
		 * Fix the terminal.  We messed up
		 * curses idea of the screen by using
		 * libedit.
		 */
		clear();
		refresh();
		cbreak();
		noecho();
		nonl();
		if (regcomp(&tmp_f_re, tmp_f_s, REG_EXTENDED)
		    != 0) {
			move(0, 0);
			printw("Invalid filter");
			refresh();
			sleep(1);
		} else {
			strlcpy(f_s, tmp_f_s, sizeof(f_s));
			f_re = tmp_f_re;
		}
		break;
	case 'F':
		f_s[0] = '\0';
		break;
	case 'g':
	case 'G':
		use_gb();
		break;
	case 'k':
	case 'K':
		use_kb();
		break;
	case 'm':
	case 'M':
		use_mb();
		break;
	case 'q':
		quit = 1;
		break;
	case 'p':
		flag_p = !flag_p;
		break;
	case 's':
		flag_s = !flag_s;
		break;
	default:
		break;
	}
	return (1);
}

int
main(int argc, char **argv)
{
	int error, i;
	int curx, cury, maxx, maxy;
	struct devstat *gsp, *gsq;
	void *sp, *sq;
	struct timespec tp, tq;
	struct gmesh gmp;
	struct gprovider *pp;
	struct gconsumer *cp;
	struct gident *gid;
	char *p;
	char ts[100], ts2[100], g_name[4096];
	long double xfer_per_sec, busy_pct;
	struct kstat krd, kwr, kfree, kother;
	uint64_t queue_len;

	memset(&kother, 0, sizeof kother);
	hist = NULL;
	el = NULL;
	curx = -1;
	loop = 0;
	/* Turn on batch mode if output is not tty. */
	if (!isatty(fileno(stdout)))
		flag_b = 1;

	f_s[0] = '\0';
	while ((i = getopt(argc, argv, "abBcdCf:gI:kmops")) != -1) {
		switch (i) {
		case 'a':
			flag_a = 1;
			break;
		case 'b':
			flag_b = 1;
			break;
		case 'B':
			flag_B = 1;
			flag_b = 1;
			break;
		case 'c':
			flag_c = 1;
			break;
		case 'C':
			flag_C = 1;
			/* csv out implies repeating batch mode */
			flag_b = 1;
			flag_B = 1;
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
			strlcpy(f_s, optarg, sizeof(f_s));
			break;
		case 'g':
			use_gb();
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
		case 'k':
			use_kb();
			break;
		case 'm':
			use_mb();
			break;
		case 'o':
			flag_o = 1;
			break;
		case 'p':
			flag_p = 1;
			break;
		case 's':
			flag_s = 1;
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
	sq = geom_stats_snapshot_get();
	if (sq == NULL)
		err(1, "geom_stats_snapshot()");
	if (!flag_b)
		setup_interactive();
	if (flag_C)
		print_head_csv();
	geom_stats_snapshot_timestamp(sq, &tq);
	for (quit = 0; !quit;) {
		loop += 1;
		sp = geom_stats_snapshot_get();
		if (sp == NULL)
			err(1, "geom_stats_snapshot()");
		geom_stats_snapshot_timestamp(sp, &tp);
		dt = tp.tv_sec - tq.tv_sec;
		dt += (tp.tv_nsec - tq.tv_nsec) * 1e-9;
		tq = tp;
		if (flag_C) { /* set timestamp string */
			(void)strftime(ts2, sizeof(ts2),
					"%F %T", localtime(&tq.tv_sec));
			assert(strlen(ts2) < 50);
			(void)snprintf(ts, sizeof(ts),
					"%s.%.9ld" ,ts2,tq.tv_nsec);
		}
		if (loop > 1 && !flag_C)
			text_header();

		geom_stats_snapshot_reset(sp);
		geom_stats_snapshot_reset(sq);
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
			if (flag_p && gid->lg_what == ISPROVIDER &&
			   ((struct gprovider *)
			    (gid->lg_ptr))->lg_geom->lg_rank != 1)
				continue;
			/* Do not print past end of window */
			if (!flag_b) {
				getyx(stdscr, cury, curx);
				if (curx > 0)
					continue;
			}
			if ((gid->lg_what == ISPROVIDER
			    || gid->lg_what == ISCONSUMER) && f_s[0] != '\0') {
				pp = gid->lg_ptr;
				if ((regexec(&f_re, pp->lg_name, 0, NULL, 0)
				     != 0))
				  continue;
			}
			if (gsp->sequence0 != gsp->sequence1) {
				/*
				 * it is ok to skip entire line silently
				 * for CSV output
				 */
				if (!flag_C)
					printer("*\n");
				continue;
			}

#define CONVENIENCE(oper, kst) \
	DSM_TRANSFERS_PER_SECOND_##oper, &kst.transfers_per_second, \
	DSM_MB_PER_SECOND_##oper, &kst.mb_per_second, \
	DSM_MS_PER_TRANSACTION_##oper, &kst.ms_per_transaction, \
	DSM_KB_PER_TRANSFER_##oper, &kst.kb_per_transfer,

			devstat_compute_statistics(gsp, gsq, dt,
			    DSM_QUEUE_LENGTH, &queue_len,
			    DSM_TRANSFERS_PER_SECOND, &xfer_per_sec,

			    CONVENIENCE(READ, krd)
			    CONVENIENCE(WRITE, kwr)
			    CONVENIENCE(FREE, kfree)

			    DSM_BUSY_PCT, &busy_pct,

			    DSM_TRANSFERS_PER_SECOND_OTHER,
				&kother.transfers_per_second,
			    DSM_MS_PER_TRANSACTION_OTHER,
				&kother.ms_per_transaction,

			    DSM_NONE);

			*gsq = *gsp;
			if (loop == 1)
				continue;

			if (flag_a && busy_pct < 0.1) {
				continue;
			}

			/* store name for geom device */
			if (gid->lg_what == ISPROVIDER) {
				pp = gid->lg_ptr;
				(void)snprintf(g_name, sizeof(g_name), "%s",
						pp->lg_name);
			} else if (gid->lg_what == ISCONSUMER) {
				cp = gid->lg_ptr;
				(void)snprintf(g_name, sizeof(g_name),
					"%s/%s/%s",
					cp->lg_geom->lg_class->lg_name,
					cp->lg_geom->lg_name,
					cp->lg_provider->lg_name);
			}

			if (flag_C) {
				printf("%s", ts); /* timestamp */
				printf(",%s", g_name); /* print name */
				printf(",%ju", (uintmax_t)queue_len);
				printf(",%.0f", (double)xfer_per_sec);
				print_kstat_csv(&krd, 0);
				print_kstat_csv(&kwr, 0);
				if (flag_d)
					print_kstat_csv(&kfree, 0);
				if (flag_o)
					print_kstat_csv(&kother, 1);
				printf(",%.1lf\n", (double)busy_pct);
			} else {
				printer(" %4ju", (uintmax_t)queue_len);
				printer(" %6.0f", (double)xfer_per_sec);

				print_kstat_text(&krd, 0);
				print_kstat_text(&kwr, 0);
				if (flag_d)
					print_kstat_text(&kfree, 0);
				if (flag_o)
					print_kstat_text(&kother, 1);
				if (!flag_b) {
					if (busy_pct > HIGH_PCT_BUSY_THRESH)
						i = 3;
					else if (busy_pct > MEDIUM_PCT_BUSY_THRESH)
						i = 2;
					else
						i = 1;
					attron(COLOR_PAIR(i));
				}
				printer("  %6.1lf", (double)busy_pct);
				if (!flag_b)
					attroff(COLOR_PAIR(i));

				printer(" %s", g_name);
				if (!flag_b)
					clrtoeol();
				printer("\n");
			}
		}
		geom_stats_snapshot_free(sp);
		if (flag_b || flag_C) {
			if (!flag_B && !flag_C && loop == 2)
				break;
			if (fflush(stdout) == EOF)
				break;
			usleep(flag_I);
		} else {
			getyx(stdscr, cury, curx);
			getmaxyx(stdscr, maxy, maxx);
			(void)maxx;
			clrtobot();
			if (maxy - 1 <= cury)
				move(maxy - 1, 0);
			refresh();

			int di = flag_I;
			while (di > 0) {
				int dii = di;
				if (dii > 100000)
					dii = 100000;
				usleep(dii);
				di -= dii;
				if (poll_input())
					break;
			}
		}
	}
	if (!flag_b) {
		el_end(el);
		endwin();
	}
	exit(EX_OK);
}
