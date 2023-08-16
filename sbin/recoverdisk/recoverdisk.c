/*-
 * SPDX-License-Identifier: Beerware
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* Safe printf into a fixed-size buffer */
#define bprintf(buf, fmt, ...)                                          \
	do {                                                            \
		int ibprintf;                                           \
		ibprintf = snprintf(buf, sizeof buf, fmt, __VA_ARGS__); \
		assert(ibprintf >= 0 && ibprintf < (int)sizeof buf);    \
	} while (0)

struct lump {
	off_t			start;
	off_t			len;
	int			state;
	TAILQ_ENTRY(lump)	list;
};

struct period {
	time_t			t0;
	time_t			t1;
	char			str[20];
	off_t			bytes_read;
	TAILQ_ENTRY(period)	list;
};
TAILQ_HEAD(period_head, period);

static volatile sig_atomic_t aborting = 0;
static int verbose = 0;
static size_t bigsize = 1024 * 1024;
static size_t medsize;
static size_t minsize = 512;
static off_t tot_size;
static off_t done_size;
static char *input;
static char *wworklist = NULL;
static char *rworklist = NULL;
static const char *unreadable_pattern = "_UNREAD_";
static const int write_errors_are_fatal = 1;
static int fdr, fdw;

static TAILQ_HEAD(, lump) lumps = TAILQ_HEAD_INITIALIZER(lumps);
static struct period_head minute = TAILQ_HEAD_INITIALIZER(minute);
static struct period_head quarter = TAILQ_HEAD_INITIALIZER(quarter);
static struct period_head hour = TAILQ_HEAD_INITIALIZER(quarter);
static struct period_head day = TAILQ_HEAD_INITIALIZER(quarter);

/**********************************************************************/

static void
report_good_read2(time_t now, size_t bytes, struct period_head *ph, time_t dt)
{
	struct period *pp;
	const char *fmt;
	struct tm tm1;

	pp = TAILQ_FIRST(ph);
	if (pp == NULL || pp->t1 < now) {
		pp = calloc(sizeof *pp, 1L);
		assert(pp != NULL);
		pp->t0 = (now / dt) * dt;
		pp->t1 = (now / dt + 1) * dt;
		assert(localtime_r(&pp->t0, &tm1) != NULL);
		if (dt < 86400)
			fmt = "%H:%M";
		else
			fmt = "%d%b";
		assert(strftime(pp->str, sizeof pp->str, fmt, &tm1) != 0);
		TAILQ_INSERT_HEAD(ph, pp, list);
	}
	pp->bytes_read += bytes;
}

static void
report_good_read(time_t now, size_t bytes)
{

	report_good_read2(now, bytes, &minute, 60L);
	report_good_read2(now, bytes, &quarter, 900L);
	report_good_read2(now, bytes, &hour, 3600L);
	report_good_read2(now, bytes, &day, 86400L);
}

static void
report_one_period(const char *period, struct period_head *ph)
{
	struct period *pp;
	int n;

	n = 0;
	printf("%s \xe2\x94\x82", period);
	TAILQ_FOREACH(pp, ph, list) {
		if (n == 3) {
			TAILQ_REMOVE(ph, pp, list);
			free(pp);
			break;
		}
		if (n++)
			printf("  \xe2\x94\x82");
		printf("  %s %14jd", pp->str, pp->bytes_read);
	}
	for (; n < 3; n++) {
		printf("  \xe2\x94\x82");
		printf("  %5s %14s", "", "");
	}
	printf("\x1b[K\n");
}

static void
report_periods(void)
{
	report_one_period("1m ", &minute);
	report_one_period("15m", &quarter);
	report_one_period("1h ", &hour);
	report_one_period("1d ", &day);
}

/**********************************************************************/

static void
set_verbose(void)
{
	struct winsize wsz;

	if (!isatty(STDIN_FILENO) || ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz))
		return;
	verbose = 1;
}

static void
report_header(int eol)
{
	printf("%13s %7s %13s %5s %13s %13s %9s",
	    "start",
	    "size",
	    "block-len",
	    "pass",
	    "done",
	    "remaining",
	    "% done");
	if (eol)
		printf("\x1b[K");
	putchar('\n');
}

#define REPORTWID 79

static void
report_hline(const char *how)
{
	int j;

	for (j = 0; j < REPORTWID; j++) {
		if (how && (j == 4 || j == 29 || j == 54)) {
			printf("%s", how);
		} else {
			printf("\xe2\x94\x80");
		}
	}
	printf("\x1b[K\n");
}

static off_t hist[REPORTWID];
static off_t last_done = -1;

static void
report_histogram(const struct lump *lp)
{
	off_t j, bucket, fp, fe, k, now;
	double a;
	struct lump *lp2;

	bucket = tot_size / REPORTWID;
	if (tot_size > bucket * REPORTWID)
		bucket += 1;
	if (done_size != last_done) {
		memset(hist, 0, sizeof hist);
		TAILQ_FOREACH(lp2, &lumps, list) {
			fp = lp2->start;
			fe = lp2->start + lp2->len;
			for (j = fp / bucket; fp < fe; j++) {
				k = (j + 1) * bucket;
				if (k > fe)
					k = fe;
				k -= fp;
				hist[j] += k;
				fp += k;
			}
		}
		last_done = done_size;
	}
	now = lp->start / bucket;
	for (j = 0; j < REPORTWID; j++) {
		a = round(8 * (double)hist[j] / bucket);
		assert (a >= 0 && a < 9);
		if (a == 0 && hist[j])
			a = 1;
		if (j == now)
			printf("\x1b[31m");
		if (a == 0) {
			putchar(' ');
		} else {
			putchar(0xe2);
			putchar(0x96);
			putchar(0x80 + (int)a);
		}
		if (j == now)
			printf("\x1b[0m");
	}
	putchar('\n');
}

static void
report(const struct lump *lp, size_t sz)
{
	struct winsize wsz;
	int j;

	assert(lp != NULL);

	if (verbose) {
		printf("\x1b[H%s\x1b[K\n", input);
		report_header(1);
	} else {
		putchar('\r');
	}

	printf("%13jd %7zu %13jd %5d %13jd %13jd %9.4f",
	    (intmax_t)lp->start,
	    sz,
	    (intmax_t)lp->len,
	    lp->state,
	    (intmax_t)done_size,
	    (intmax_t)(tot_size - done_size),
	    100*(double)done_size/(double)tot_size
	);

	if (verbose) {
		printf("\x1b[K\n");
		report_hline(NULL);
		report_histogram(lp);
		if (TAILQ_EMPTY(&minute)) {
			report_hline(NULL);
		} else {
			report_hline("\xe2\x94\xac");
			report_periods();
			report_hline("\xe2\x94\xb4");
		}
		j = ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz);
		if (!j)
			printf("\x1b[%d;1H", wsz.ws_row);
	}
	fflush(stdout);
}

/**********************************************************************/

static void
new_lump(off_t start, off_t len, int state)
{
	struct lump *lp;

	lp = malloc(sizeof *lp);
	if (lp == NULL)
		err(1, "Malloc failed");
	lp->start = start;
	lp->len = len;
	lp->state = state;
	TAILQ_INSERT_TAIL(&lumps, lp, list);
}

/**********************************************************************
 * Save the worklist if -w was given
 */

static void
save_worklist(void)
{
	FILE *file;
	struct lump *llp;
	char buf[PATH_MAX];

	if (fdw >= 0 && fdatasync(fdw))
		err(1, "Write error, probably disk full");

	if (wworklist != NULL) {
		bprintf(buf, "%s.tmp", wworklist);
		(void)fprintf(stderr, "\nSaving worklist ...");
		(void)fflush(stderr);

		file = fopen(buf, "w");
		if (file == NULL)
			err(1, "Error opening file %s", buf);

		TAILQ_FOREACH(llp, &lumps, list)
			fprintf(file, "%jd %jd %d\n",
			    (intmax_t)llp->start, (intmax_t)llp->len,
			    llp->state);
		(void)fflush(file);
		if (ferror(file) || fdatasync(fileno(file)) || fclose(file))
			err(1, "Error writing file %s", buf);
		if (rename(buf, wworklist))
			err(1, "Error renaming %s to %s", buf, wworklist);
		(void)fprintf(stderr, " done.\n");
	}
}

/* Read the worklist if -r was given */
static off_t
read_worklist(off_t t)
{
	off_t s, l, d;
	int state, lines;
	FILE *file;

	(void)fprintf(stderr, "Reading worklist ...");
	(void)fflush(stderr);
	file = fopen(rworklist, "r");
	if (file == NULL)
		err(1, "Error opening file %s", rworklist);

	lines = 0;
	d = t;
	for (;;) {
		++lines;
		if (3 != fscanf(file, "%jd %jd %d\n", &s, &l, &state)) {
			if (!feof(file))
				err(1, "Error parsing file %s at line %d",
				    rworklist, lines);
			else
				break;
		}
		new_lump(s, l, state);
		d -= l;
	}
	if (fclose(file))
		err(1, "Error closing file %s", rworklist);
	(void)fprintf(stderr, " done.\n");
	/*
	 * Return the number of bytes already read
	 * (at least not in worklist).
	 */
	return (d);
}

/**********************************************************************/

static void
write_buf(int fd, const void *buf, ssize_t len, off_t where)
{
	ssize_t i;

	i = pwrite(fd, buf, len, where);
	if (i == len)
		return;

	printf("\nWrite error at %jd/%zu\n\t%s\n",
	    where, i, strerror(errno));
	save_worklist();
	if (write_errors_are_fatal)
		exit(3);
}

static void
fill_buf(char *buf, ssize_t len, const char *pattern)
{
	ssize_t sz = strlen(pattern);
	ssize_t i, j;

	for (i = 0; i < len; i += sz) {
		j = len - i;
		if (j > sz)
			j = sz;
		memcpy(buf + i, pattern, j);
	}
}

/**********************************************************************/

static void
usage(void)
{
	(void)fprintf(stderr, "usage: recoverdisk [-b bigsize] [-r readlist] "
	    "[-s interval] [-w writelist] source [destination]\n");
	/* XXX update */
	exit(1);
}

static void
sighandler(__unused int sig)
{

	aborting = 1;
}

int
main(int argc, char * const argv[])
{
	int ch;
	size_t sz, j;
	int error;
	char *buf;
	u_int sectorsize;
	off_t stripesize;
	time_t t1, t2;
	struct stat sb;
	u_int n, snapshot = 60;
	static struct lump *lp;

	while ((ch = getopt(argc, argv, "b:r:w:s:u:v")) != -1) {
		switch (ch) {
		case 'b':
			bigsize = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			rworklist = strdup(optarg);
			if (rworklist == NULL)
				err(1, "Cannot allocate enough memory");
			break;
		case 's':
			snapshot = strtoul(optarg, NULL, 0);
			break;
		case 'u':
			unreadable_pattern = optarg;
			break;
		case 'v':
			set_verbose();
			break;
		case 'w':
			wworklist = strdup(optarg);
			if (wworklist == NULL)
				err(1, "Cannot allocate enough memory");
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2)
		usage();

	input = argv[0];
	fdr = open(argv[0], O_RDONLY);
	if (fdr < 0)
		err(1, "Cannot open read descriptor %s", argv[0]);

	error = fstat(fdr, &sb);
	if (error < 0)
		err(1, "fstat failed");
	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
		error = ioctl(fdr, DIOCGSECTORSIZE, &sectorsize);
		if (error < 0)
			err(1, "DIOCGSECTORSIZE failed");

		error = ioctl(fdr, DIOCGSTRIPESIZE, &stripesize);
		if (error == 0 && stripesize > sectorsize)
			sectorsize = stripesize;

		minsize = sectorsize;
		bigsize = rounddown(bigsize, sectorsize);

		error = ioctl(fdr, DIOCGMEDIASIZE, &tot_size);
		if (error < 0)
			err(1, "DIOCGMEDIASIZE failed");
	} else {
		tot_size = sb.st_size;
	}

	if (bigsize < minsize)
		bigsize = minsize;

	for (ch = 0; (bigsize >> ch) > minsize; ch++)
		continue;
	medsize = bigsize >> (ch / 2);
	medsize = rounddown(medsize, minsize);

	fprintf(stderr, "Bigsize = %zu, medsize = %zu, minsize = %zu\n",
	    bigsize, medsize, minsize);

	buf = malloc(bigsize);
	if (buf == NULL)
		err(1, "Cannot allocate %zu bytes buffer", bigsize);

	if (argc > 1) {
		fdw = open(argv[1], O_WRONLY | O_CREAT, DEFFILEMODE);
		if (fdw < 0)
			err(1, "Cannot open write descriptor %s", argv[1]);
		if (ftruncate(fdw, tot_size) < 0)
			err(1, "Cannot truncate output %s to %jd bytes",
			    argv[1], (intmax_t)tot_size);
	} else
		fdw = -1;

	if (rworklist != NULL) {
		done_size = read_worklist(tot_size);
	} else {
		new_lump(0, tot_size, 0);
		done_size = 0;
	}
	if (wworklist != NULL)
		signal(SIGINT, sighandler);

	t1 = time(NULL);
	sz = 0;
	if (!verbose)
		report_header(0);
	else
		printf("\x1b[2J");
	n = 0;
	for (;;) {
		lp = TAILQ_FIRST(&lumps);
		if (lp == NULL)
			break;
		while (lp->len > 0) {

			if (lp->state == 0)
				sz = MIN(lp->len, (off_t)bigsize);
			else if (lp->state == 1)
				sz = MIN(lp->len, (off_t)medsize);
			else
				sz = MIN(lp->len, (off_t)minsize);
			assert(sz != 0);

			t2 = time(NULL);
			if (t1 != t2 || lp->len < (off_t)bigsize) {
				t1 = t2;
				if (++n == snapshot) {
					save_worklist();
					n = 0;
				}
				report(lp, sz);
			}

			j = pread(fdr, buf, sz, lp->start);
#if 0
if (!(random() & 0xf)) {
	j = -1;
	errno = EIO;
}
#endif
			if (j == sz) {
				done_size += sz;
				if (fdw >= 0)
					write_buf(fdw, buf, sz, lp->start);
				lp->start += sz;
				lp->len -= sz;
				if (verbose && lp->state > 2)
					report_good_read(t2, sz);
				continue;
			}
			error = errno;

			printf("%jd %zu %d read error (%s)\n",
			    lp->start, sz, lp->state, strerror(error));
			if (verbose)
				report(lp, sz);
			if (fdw >= 0 && strlen(unreadable_pattern)) {
				fill_buf(buf, sz, unreadable_pattern);
				write_buf(fdw, buf, sz, lp->start);
			}
			new_lump(lp->start, sz, lp->state + 1);
			lp->start += sz;
			lp->len -= sz;
			if (error == EINVAL) {
				printf("Try with -b 131072 or lower ?\n");
				aborting = 1;
				break;
			}
			if (error == ENXIO) {
				printf("Input device probably detached...\n");
				aborting = 1;
				break;
			}
		}
		if (aborting)
			save_worklist();
		if (aborting || !TAILQ_NEXT(lp, list))
			report(lp, sz);
		if (aborting)
			break;
		assert(lp->len == 0);
		TAILQ_REMOVE(&lumps, lp, list);
		free(lp);
	}
	printf("%s", aborting ? "Aborted\n" : "Completed\n");
	free(buf);
	return (0);
}
