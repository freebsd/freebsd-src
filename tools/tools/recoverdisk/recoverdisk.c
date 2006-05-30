/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t aborting = 0;
static size_t bigsize = 1024 * 1024;
static size_t medsize = 64 * 1024;
static size_t minsize = 512;

struct lump {
	off_t			start;
	off_t			len;
	int			state;
	TAILQ_ENTRY(lump)	list;
};

static TAILQ_HEAD(, lump) lumps = TAILQ_HEAD_INITIALIZER(lumps);

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

static struct lump *lp;
static char *wworklist = NULL;
static char *rworklist = NULL;

/* Save the worklist if -w was given */
static void
save_worklist(void)
{
	FILE *file;

	if (wworklist != NULL) {
		(void)fprintf(stderr, "\nSaving worklist ...");
		fflush(stderr);

		file = fopen(wworklist, "w");
		if (file == NULL)
			err(1, "Error opening file %s", wworklist);

		for (;;) {
			lp = TAILQ_FIRST(&lumps);
			if (lp == NULL)
				break;
			fprintf(file, "%jd %jd %d\n",
			    (intmax_t)lp->start, (intmax_t)lp->len, lp->state);
			TAILQ_REMOVE(&lumps, lp, list);
		}
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
	fflush(stderr);
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
	(void)fprintf(stderr, " done.\n");
	/*
	 * Return the number of bytes already read
	 * (at least not in worklist).
	 */
	return (d);
}

static void
usage(void)
{
	(void)fprintf(stderr,
    "usage: recoverdisk [-r worklist] [-w worklist] source-drive [destination]\n");
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
	int fdr, fdw;
	off_t t, d;
	size_t i, j;
	int error, flags;
	u_char *buf;
	u_int sectorsize;
	time_t t1, t2;
	struct stat sb;

	while ((ch = getopt(argc, argv, "r:w:")) != -1) {
		switch (ch) {
		case 'r':
			rworklist = strdup(optarg);
			if (rworklist == NULL)
				err(1, "Cannot allocate enough memory");
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

	fdr = open(argv[0], O_RDONLY);
	if (fdr < 0)
		err(1, "Cannot open read descriptor %s", argv[0]);

	error = fstat(fdr, &sb);
	if (error < 0)
		err(1, "fstat failed");
	flags = O_WRONLY;
	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
		error = ioctl(fdr, DIOCGSECTORSIZE, &sectorsize);
		if (error < 0)
			err(1, "DIOCGSECTORSIZE failed");

		/*
		 * Make medsize roughly 64kB, depending on native sector
		 * size. bigsize has to be a multiple of medsize.
		 * For media with 2352 sectors, this will
		 * result in 2352, 63504, and 1016064 bytes.
		 */
		minsize = sectorsize;
		medsize = (medsize / sectorsize) * sectorsize;
		bigsize = medsize * 16;

		error = ioctl(fdr, DIOCGMEDIASIZE, &t);
		if (error < 0)
			err(1, "DIOCGMEDIASIZE failed");
	} else {
		t = sb.st_size;
		flags |= O_CREAT | O_TRUNC;
	}

	buf = malloc(bigsize);
	if (buf == NULL)
		err(1, "Cannot allocate %jd bytes buffer", (intmax_t)bigsize);

	if (argc > 1) {
		fdw = open(argv[1], flags, DEFFILEMODE);
		if (fdw < 0)
			err(1, "Cannot open write descriptor %s", argv[1]);
	} else
		fdw = -1;

	if (rworklist != NULL) {
		d = read_worklist(t);
	} else {
		new_lump(0, t, 0);
		d = 0;
	}
	if (wworklist != NULL)
		signal(SIGINT, sighandler);

	t1 = 0;
	printf("%13s %7s %13s %5s %13s %13s %9s\n",
	    "start", "size", "len", "state", "done", "remaining", "% done");
	for (;;) {
		lp = TAILQ_FIRST(&lumps);
		if (lp == NULL)
			break;
		while (lp->len > 0 && !aborting) {
			i = MIN(lp->len, (off_t)bigsize);
			if (lp->state == 1)
				i = MIN(lp->len, (off_t)medsize);
			if (lp->state > 1)
				i = MIN(lp->len, (off_t)minsize);
			time(&t2);
			if (t1 != t2 || lp->len < (off_t)bigsize) {
				printf("\r%13jd %7zu %13jd %5d %13jd %13jd %.7f",
				    (intmax_t)lp->start,
				    i, 
				    (intmax_t)lp->len,
				    lp->state,
				    (intmax_t)d,
				    (intmax_t)(t - d),
				    (double)d/(double)t);
				t1 = t2;
			}
			if (i == 0) {
				errx(1, "BOGUS i %10jd", (intmax_t)i);
			}
			fflush(stdout);
			j = pread(fdr, buf, i, lp->start);
			if (j == i) {
				d += i;
				if (fdw >= 0)
					j = pwrite(fdw, buf, i, lp->start);
				else
					j = i;
				if (j != i)
					printf("\nWrite error at %jd/%zu\n",
					    lp->start, i);
				lp->start += i;
				lp->len -= i;
				continue;
			}
			printf("\n%jd %zu failed %d\n", lp->start, i, errno);
			new_lump(lp->start, i, lp->state + 1);
			lp->start += i;
			lp->len -= i;
		}
		if (aborting) {
			save_worklist();
			return (0);
		}
		TAILQ_REMOVE(&lumps, lp, list);
		free(lp);
	}
	printf("\nCompleted\n");
	return (0);
}
