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

/*
 * This is a compromise between speed and wasted effort
 */
#define COMPROMISE_SIZE		(128<<10)

struct lump {
	uint64_t		start;
	uint64_t		len;
	unsigned		pass;
	TAILQ_ENTRY(lump)	list;
};

struct period {
	time_t			t0;
	time_t			t1;
	char			str[20];
	uint64_t		bytes_read;
	TAILQ_ENTRY(period)	list;
};
TAILQ_HEAD(period_head, period);

static volatile sig_atomic_t aborting = 0;
static int verbose = 0;
static uint64_t big_read;
static uint64_t medium_read;
static uint64_t small_read;
static uint64_t total_size;
static uint64_t done_size;
static uint64_t wasted_size;
static char *input;
static char *write_worklist_file = NULL;
static char *read_worklist_file = NULL;
static const char *unreadable_pattern = "_UNREAD_";
static int write_errors_are_fatal = 1;
static int read_fd, write_fd;
static FILE *log_file = NULL;
static char *work_buf;
static char *pattern_buf;
static double error_pause;
static double interval;

static unsigned nlumps;
static double n_reads, n_good_reads;
static time_t t_first;
static TAILQ_HEAD(, lump) lumps = TAILQ_HEAD_INITIALIZER(lumps);
static struct period_head minute = TAILQ_HEAD_INITIALIZER(minute);
static struct period_head quarter = TAILQ_HEAD_INITIALIZER(quarter);
static struct period_head hour = TAILQ_HEAD_INITIALIZER(quarter);
static struct period_head day = TAILQ_HEAD_INITIALIZER(quarter);

/**********************************************************************/

static void
account_good_read_period(time_t now, uint64_t bytes,
    struct period_head *ph, time_t dt)
{
	struct period *pp;
	const char *fmt;
	struct tm tm1;

	pp = TAILQ_FIRST(ph);
	if (pp == NULL || pp->t1 < now) {
		pp = calloc(1UL, sizeof(*pp));
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
account_good_read(time_t now, uint64_t bytes)
{

	account_good_read_period(now, bytes, &minute, 60L);
	account_good_read_period(now, bytes, &quarter, 900L);
	account_good_read_period(now, bytes, &hour, 3600L);
	account_good_read_period(now, bytes, &day, 86400L);
}

static void
report_one_period(const char *period, struct period_head *ph)
{
	struct period *pp;
	int n;

	n = 0;
	printf("%s ", period);
	TAILQ_FOREACH(pp, ph, list) {
		if (++n == 4) {
			TAILQ_REMOVE(ph, pp, list);
			free(pp);
			break;
		}
		printf("\xe2\x94\x82  %s %14ju  ",
		    pp->str, (uintmax_t)pp->bytes_read);
	}
	for (; n < 3; n++) {
		printf("\xe2\x94\x82  %5s %14s  ", "", "");
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

	verbose = 1;
}

static void
report_header(const char *term)
{
	printf("%13s %7s %13s %5s %13s %13s %9s%s",
	    "start",
	    "size",
	    "block-len",
	    "pass",
	    "done",
	    "remaining",
	    "% done",
	    term
	);
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

static uint64_t hist[REPORTWID];
static uint64_t prev_done = ~0UL;

static void
report_histogram(uint64_t start)
{
	uint64_t j, bucket, fp, fe, k, now;
	double a;
	struct lump *lp2;

	bucket = total_size / REPORTWID;
	if (total_size > bucket * REPORTWID)
		bucket += 1;
	if (done_size != prev_done) {
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
		prev_done = done_size;
	}
	now = start / bucket;
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
			putchar(0x80 + (char)a);
		}
		if (j == now)
			printf("\x1b[0m");
	}
	putchar('\n');
}

static void
report(uint64_t sz)
{
	struct winsize wsz;
	const struct lump *lp = TAILQ_FIRST(&lumps);
	int j;
	unsigned pass = 0;
	uintmax_t start = 0, length = 0;
	time_t t_now = time(NULL);

	if (lp != NULL) {
		pass = lp->pass;
		start = lp->start;
		length = lp->len;
	}

	if (verbose) {
		printf("\x1b[H%s\x1b[K\n", input);
		report_header("\x1b[K\n");
	}

	printf("%13ju %7ju %13ju %5u %13ju %13ju %9.4f",
	    start,
	    (uintmax_t)sz,
	    length,
	    pass,
	    (uintmax_t)done_size,
	    (uintmax_t)(total_size - done_size),
	    100*(double)done_size/(double)total_size
	);

	if (verbose) {
		printf("\x1b[K\n");
		report_hline(NULL);
		report_histogram(start);
		if (TAILQ_EMPTY(&minute)) {
			report_hline(NULL);
		} else {
			report_hline("\xe2\x94\xac");
			report_periods();
			report_hline("\xe2\x94\xb4");
		}
		printf("Missing: %u", nlumps);
		printf("  Success: %.0f/%.0f =", n_good_reads, n_reads);
		printf(" %.4f%%", 100 * n_good_reads / n_reads);
		printf("  Duration: %.3fs", (t_now - t_first) / n_reads);
		printf("\x1b[K\n");
		report_hline(NULL);
		j = ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz);
		if (!j)
			printf("\x1b[%d;1H", wsz.ws_row);
	} else {
		printf("\n");
	}
}

/**********************************************************************/

static void
new_lump(uint64_t start, uint64_t len, unsigned pass)
{
	struct lump *lp;

	assert(len > 0);
	lp = malloc(sizeof *lp);
	if (lp == NULL)
		err(1, "Malloc failed");
	lp->start = start;
	lp->len = len;
	lp->pass = pass;
	TAILQ_INSERT_TAIL(&lumps, lp, list);
	nlumps += 1;
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

	if (write_fd >= 0 && fdatasync(write_fd))
		err(1, "Write error, probably disk full");

	if (write_worklist_file != NULL) {
		snprintf(buf, sizeof(buf), "%s.tmp", write_worklist_file);
		fprintf(stderr, "\nSaving worklist ...");

		file = fopen(buf, "w");
		if (file == NULL)
			err(1, "Error opening file %s", buf);

		TAILQ_FOREACH(llp, &lumps, list) {
			assert (llp->len > 0);
			fprintf(file, "%ju %ju %u\n",
			    (uintmax_t)llp->start,
			    (uintmax_t)llp->len,
			    llp->pass);
		}
		fflush(file);
		if (ferror(file) || fdatasync(fileno(file)) || fclose(file))
			err(1, "Error writing file %s", buf);
		if (rename(buf, write_worklist_file))
			err(1, "Error renaming %s to %s",
			    buf, write_worklist_file);
		fprintf(stderr, " done.\n");
	}
}

/* Read the worklist if -r was given */
static uint64_t
read_worklist(void)
{
	uintmax_t start, length;
	uint64_t missing = 0;
	unsigned pass, lines;
	FILE *file;

	fprintf(stderr, "Reading worklist ...");
	file = fopen(read_worklist_file, "r");
	if (file == NULL)
		err(1, "Error opening file %s", read_worklist_file);

	lines = 0;
	for (;;) {
		++lines;
		if (3 != fscanf(file, "%ju %ju %u\n", &start, &length, &pass)) {
			if (!feof(file))
				err(1, "Error parsing file %s at line %u",
				    read_worklist_file, lines);
			else
				break;
		}
		if (length > 0) {
			new_lump(start, length, pass);
			missing += length;
		}
	}
	if (fclose(file))
		err(1, "Error closing file %s", read_worklist_file);
	fprintf(stderr, " done.\n");
	/*
	 * Return the number of bytes outstanding
	 */
	return (missing);
}

/**********************************************************************/

static void
write_buf(int fd, const void *buf, uint64_t length, uint64_t where)
{
	int64_t i;

	i = pwrite(fd, buf, length, (off_t)where);
	if (i > 0 && (uint64_t)i == length)
		return;

	printf("\nWrite error at %ju/%ju: %jd (%s)\n",
	    (uintmax_t)where,
	    (uintmax_t)length,
	    (intmax_t)i, strerror(errno));
	save_worklist();
	if (write_errors_are_fatal)
		exit(3);
}

static void
fill_buf(char *buf, int64_t len, const char *pattern)
{
	int64_t sz = strlen(pattern);
	int64_t i;

	for (i = 0; i < len; i += sz) {
		memcpy(buf + i, pattern, MIN(len - i, sz));
	}
}

/**********************************************************************/

static void
usage(void)
{
	fprintf(stderr, "usage: recoverdisk "
	    "[-b big_read] [-i interval ] [-r readlist] "
	    "[-s interval] [-w writelist] source [destination]\n");
	/* XXX update */
	exit(1);
}

static void
sighandler(int sig)
{

	(void)sig;
	aborting = 1;
}

/**********************************************************************/

static int64_t
attempt_one_lump(time_t t_now)
{
	struct lump *lp;
	uint64_t sz;
	int64_t retval;
	int error;

	lp = TAILQ_FIRST(&lumps);
	if (lp == NULL)
		return(0);

	if (lp->pass == 0) {
		sz = MIN(lp->len, big_read);
	} else if (lp->pass == 1) {
		sz = MIN(lp->len, medium_read);
	} else {
		sz = MIN(lp->len, small_read);
	}

	assert(sz != 0);

	n_reads += 1;
	retval = pread(read_fd, work_buf, sz, lp->start);

#if 0 /* enable this when testing */
	if (!(random() & 0xf)) {
		retval = -1;
		errno = EIO;
		usleep(20000);
	} else {
		usleep(2000);
	}
#endif

	error = errno;
	if (retval > 0) {
		n_good_reads += 1;
		sz = retval;
		done_size += sz;
		if (write_fd >= 0) {
			write_buf(write_fd, work_buf, sz, lp->start);
		}
		if (log_file != NULL) {
			fprintf(log_file, "%jd %ju %ju\n",
			    (intmax_t)t_now,
			    (uintmax_t)lp->start,
			    (uintmax_t)sz
			);
			fflush(log_file);
		}
	} else {
		wasted_size += sz;
		printf("%14ju %7ju read error %d: (%s)",
		    (uintmax_t)lp->start,
		    (uintmax_t)sz, error, strerror(error));
		if (error_pause > 1) {
			printf(" (Pausing %g s)", error_pause);
		}
		printf("\n");

		if (write_fd >= 0 && pattern_buf != NULL) {
			write_buf(write_fd, pattern_buf, sz, lp->start);
		}
		new_lump(lp->start, sz, lp->pass + 1);
		retval = -sz;
	}
	lp->start += sz;
	lp->len -= sz;
	if (lp->len == 0) {
		TAILQ_REMOVE(&lumps, lp, list);
		nlumps -= 1;
		free(lp);
	}
	errno = error;
	return (retval);
}


/**********************************************************************/

static void
determine_total_size(void)
{
	struct stat sb;
	int error;

	if (total_size != 0)
		return;

	error = fstat(read_fd, &sb);
	if (error < 0)
		err(1, "fstat failed");

	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
#ifdef DIOCGMEDIASIZE
		off_t mediasize;
		error = ioctl(read_fd, DIOCGMEDIASIZE, &mediasize);
		if (error == 0 && mediasize > 0) {
			total_size = mediasize;
			printf("# Got total_size from DIOCGMEDIASIZE: %ju\n",
			    (uintmax_t)total_size);
			return;
		}
#endif
	} else if (S_ISREG(sb.st_mode) && sb.st_size > 0) {
		total_size = sb.st_size;
		printf("# Got total_size from stat(2): %ju\n",
		    (uintmax_t)total_size);
		return;
	} else {
		errx(1, "Input must be device or regular file");
	}
	fprintf(stderr, "Specify total size with -t option\n");
	exit(1);
}

static void
determine_read_sizes(void)
{
	int error;
	u_int sectorsize;
	off_t stripesize;

#ifdef DIOCGSECTORSIZE
	if (small_read == 0) {
		error = ioctl(read_fd, DIOCGSECTORSIZE, &sectorsize);
		if (error >= 0 && sectorsize > 0) {
			small_read = sectorsize;
			printf("# Got small_read from DIOCGSECTORSIZE: %ju\n",
			    (uintmax_t)small_read
			);
		}
	}
#endif

	if (small_read == 0) {
		small_read = 512;
		printf("# Defaulting small_read to %ju\n", (uintmax_t)small_read);
	}

	if (medium_read && (medium_read % small_read)) {
		errx(1,
		    "medium_read (%ju) is not a multiple of small_read (%ju)\n",
		    (uintmax_t)medium_read, (uintmax_t)small_read
		);
	}

	if (big_read != 0 && (big_read % small_read)) {
		errx(1,
		    "big_read (%ju) is not a multiple of small_read (%ju)\n",
		    (uintmax_t)big_read, (uintmax_t)small_read
		);
	}

#ifdef DIOCGSTRIPESIZE
	if (medium_read == 0) {
		error = ioctl(read_fd, DIOCGSTRIPESIZE, &stripesize);
		if (error < 0 || stripesize <= 0) {
			// nope
		} else if ((uint64_t)stripesize < small_read) {
			// nope
		} else if (stripesize % small_read) {
			// nope
		} else if (stripesize <= COMPROMISE_SIZE) {
			medium_read = stripesize;
			printf("# Got medium_read from DIOCGSTRIPESIZE: %ju\n",
			    (uintmax_t)medium_read
			);
		}
	}
#endif

#if defined(DIOCGFWSECTORS) && defined(DIOCGFWHEADS)
	if (medium_read == 0) {
		u_int fwsectors = 0, fwheads = 0;
		error = ioctl(read_fd, DIOCGFWSECTORS, &fwsectors);
		if (error)
			fwsectors = 0;
		error = ioctl(read_fd, DIOCGFWHEADS, &fwheads);
		if (error)
			fwheads = 0;
		if (fwsectors * fwheads * small_read <= COMPROMISE_SIZE) {
			medium_read = fwsectors * fwheads * small_read;
			printf(
			    "# Got medium_read from DIOCGFW{SECTORS*HEADS}: %ju\n",
			    (uintmax_t)medium_read
			);
		} else if (fwsectors * small_read <= COMPROMISE_SIZE) {
			medium_read = fwsectors * small_read;
			printf(
			    "# Got medium_read from DIOCGFWSECTORS: %ju\n",
			    (uintmax_t)medium_read
			);
		}
	}
#endif

	if (big_read == 0 && medium_read != 0) {
		if (medium_read * 2 > COMPROMISE_SIZE) {
			big_read = medium_read;
			medium_read = 0;
		} else {
			big_read = COMPROMISE_SIZE;
			big_read -= big_read % medium_read;
		}
		printf("# Got big_read from medium_read: %ju\n",
		    (uintmax_t)big_read
		);
	}

	if (big_read == 0) {
		big_read = COMPROMISE_SIZE;
		big_read -= big_read % small_read;
		printf("# Defaulting big_read to %ju\n",
		    (uintmax_t)big_read
		);
	}

	if (medium_read >= big_read)
		medium_read = 0;

	if (medium_read == 0) {
		/*
		 * We do not want to go directly to single sectors, but
		 * we also dont want to waste time doing multi-sector
		 * reads with high failure probability.
		 */
		uint64_t h = big_read;
		uint64_t l = small_read;
		while (h > l) {
			h >>= 2;
			l <<= 1;
		}
		medium_read = h;
		printf("# Got medium_read from small_read & big_read: %ju\n",
		    (uintmax_t)medium_read
		);
	}
	printf("# Bigsize = %ju, medium_read = %ju, small_read = %ju\n",
	    (uintmax_t)big_read, (uintmax_t)medium_read, (uintmax_t)small_read);

	assert(0 < small_read);

	assert(0 < medium_read);
	assert(medium_read >= small_read);
	assert(medium_read <= big_read);
	assert(medium_read % small_read == 0);

	assert(0 < big_read);
	assert(big_read >= medium_read);
	assert(big_read % small_read == 0);
}

/**********************************************************************/

static void
monitor_read_sizes(uint64_t failed_size)
{

	if (failed_size == big_read && medium_read != small_read) {
		if (n_reads < n_good_reads + 3)
			return;
		fprintf(
		    stderr,
		    "Too many failures for big reads."
		    " (%.0f bad of %.0f)"
		    " Shifting to medium_reads.\n",
		    n_reads - n_good_reads, n_reads
		);
		big_read = medium_read;
		medium_read = small_read;
		wasted_size = 0;
		return;
	}

	if (big_read > small_read && wasted_size / small_read > 200) {
		fprintf(
		    stderr,
		    "Too much wasted effort."
		    " (%.0f bad of %.0f)"
		    " Shifting to small_reads.\n",
		    n_reads - n_good_reads, n_reads
		);
		big_read = small_read;
		medium_read = small_read;
		return;
	}
}

/**********************************************************************/

int
main(int argc, char * const argv[])
{
	int ch;
	int64_t sz;
	int error;
	time_t t_now, t_report, t_save;
	time_t snapshot = 60, unsaved;
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	while ((ch = getopt(argc, argv, "b:i:l:p:m:r:w:s:t:u:v")) != -1) {
		switch (ch) {
		case 'b':
			big_read = strtoul(optarg, NULL, 0);
			break;
		case 'i':
			interval = strtod(optarg, NULL);
			break;
		case 'l':
			log_file = fopen(optarg, "a");
			if (log_file == NULL) {
				err(1, "Could not open logfile for append");
			}
			break;
		case 'p':
			error_pause = strtod(optarg, NULL);
			break;
		case 'm':
			medium_read = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			read_worklist_file = strdup(optarg);
			if (read_worklist_file == NULL)
				err(1, "Cannot allocate enough memory");
			break;
		case 's':
			small_read = strtoul(optarg, NULL, 0);
			break;
		case 't':
			total_size = strtoul(optarg, NULL, 0);
			break;
		case 'u':
			unreadable_pattern = optarg;
			break;
		case 'v':
			set_verbose();
			break;
		case 'w':
			write_worklist_file = strdup(optarg);
			if (write_worklist_file == NULL)
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
	read_fd = open(argv[0], O_RDONLY);
	if (read_fd < 0)
		err(1, "Cannot open read descriptor %s", argv[0]);

	determine_total_size();

	determine_read_sizes();

	work_buf = malloc(big_read);
	assert (work_buf != NULL);

	if (argc > 1) {
		write_fd = open(argv[1], O_WRONLY | O_CREAT, DEFFILEMODE);
		if (write_fd < 0)
			err(1, "Cannot open write descriptor %s", argv[1]);
		if (ftruncate(write_fd, (off_t)total_size) < 0)
			err(1, "Cannot truncate output %s to %ju bytes",
			    argv[1], (uintmax_t)total_size);
	} else {
		write_fd = -1;
	}

	if (strlen(unreadable_pattern)) {
		pattern_buf = malloc(big_read);
		assert(pattern_buf != NULL);
		fill_buf(pattern_buf, big_read, unreadable_pattern);
	}

	if (read_worklist_file != NULL) {
		done_size = total_size - read_worklist();
	} else {
		new_lump(0UL, total_size, 0UL);
		done_size = 0;
	}
	if (write_worklist_file != NULL)
		signal(SIGINT, sighandler);

	sz = 0;
	if (!verbose)
		report_header("\n");
	else
		printf("\x1b[2J");

	t_first = time(NULL);
	t_report = t_first;
	t_save = t_first;
	unsaved = 0;
	while (!aborting) {
		if (interval > 0) {
			usleep((unsigned long)(1e6 * interval));
		}
		t_now = time(NULL);
		sz = attempt_one_lump(t_now);
		error = errno;

		if (sz == 0) {
			break;
		}

		if (sz > 0) {
			unsaved += 1;
		}
		if (unsaved && (t_save + snapshot) < t_now) {
			save_worklist();
			unsaved = 0;
			t_save = t_now;
			if (!verbose) {
				report_header("\n");
				t_report = t_now;
			}
		}
		if (sz > 0) {
			if (verbose) {
				account_good_read(t_now, sz);
			}
			if (t_report != t_now) {
				report(sz);
				t_report = t_now;
			}
			continue;
		}

		monitor_read_sizes(-sz);

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
		report(-sz);
		t_report = t_now;
		if (error_pause > 0) {
			usleep((unsigned long)(1e6 * error_pause));
		}
	}
	save_worklist();
	free(work_buf);
	if (pattern_buf != NULL)
		free(pattern_buf);
	printf("%s", aborting ? "Aborted\n" : "Completed\n");
	report(0UL);
	return (0);	// XXX
}
