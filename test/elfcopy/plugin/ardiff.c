/* Selectively compare two ar archives.
 * Usage:
 * 	ardiff [-ni] [-t name] ar1 ar2
 * Options:
 * 	-c compare member content. (This implies -s)
 * 	-n compare member name.
 * 	-i compare member mtime.
 *	-l compare archive length (member count).
 *	-s compare member size.
 *	-t specify the test name.
 *
 * By default, it compares nothing and consider the test "not ok"
 * iff it encounters errors while reading archive.
 *
 * $Id: ardiff.c 3366 2016-01-24 21:33:06Z jkoshy $
 */

#include <archive.h>
#include <archive_entry.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNTER	"/tmp/bsdar-test-total"
#define PASSED	"/tmp/bsdar-test-passed"

static void	usage(void);
static void	filediff(const char *tc, const char *msg, const char *e);
static void	filesame(const char *tc);
static void	incct(const char *pathname);

int
main(int argc, char **argv)
{
	struct archive *a1;
	struct archive *a2;
	struct archive_entry *e1;
	struct archive_entry *e2;
	const char *tc;
	char *buf1;
	char *buf2;
	char checkcont;
	char checklen;
	char checkname;
	char checksize;
	char checktime;
	char a1end;
	size_t size1;
	size_t size2;
	int opt, r;

	/*
	 * Parse command line options.
	 */
	checkcont = 0;
	checklen = 0;
	checkname = 0;
	checksize = 0;
	checktime = 0;
	tc = NULL;
	while ((opt = getopt(argc, argv, "cilnst:")) != -1) {
		switch(opt) {
		case 'c':
			checkcont = 1;
			break;
		case 'i':
			checktime = 1;
			break;
		case 'l':
			checklen = 1;
			break;
		case 'n':
			checkname = 1;
			break;
		case 's':
			checksize = 1;
		case 't':
			tc = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	/* Open file 1 */
	a1 = archive_read_new();
	archive_read_support_format_ar(a1);
	if (archive_read_open_filename(a1, argv[0],
	    1024*10)) {
		warnx("%s", archive_error_string(a1));
		filediff(tc, "archive open failed", NULL);
	}

	/* Open file 2 */
	a2 = archive_read_new();
	archive_read_support_format_ar(a2);
	if (archive_read_open_filename(a2, argv[1],
	    1024*10)) {
		warnx("%s", archive_error_string(a2));
		filediff(tc, "archive open failed", NULL);
	}

	/* Main loop */
	a1end = 0;
	size1 = 0;
	size2 = 0;
	for (;;) {
		/*
		 * Read header from each archive, compare length.
		 */
		r = archive_read_next_header(a1, &e1);
		if (r == ARCHIVE_EOF)
			a1end = 1;
		if (r == ARCHIVE_WARN || r == ARCHIVE_RETRY ||
		    r == ARCHIVE_FATAL) {
			warnx("%s", archive_error_string(a1));
			filediff(tc, "archive data error", NULL);
		}
		r = archive_read_next_header(a2, &e2);
		if (r == ARCHIVE_EOF) {
			if (a1end > 0)
				break;
			else {
				if (checklen)
					filediff(tc, "length differ", NULL);
				break;
			}
		}
		if (r == ARCHIVE_WARN || r == ARCHIVE_RETRY ||
		    r == ARCHIVE_FATAL) {
			warnx("%s", archive_error_string(a2));
			filediff(tc, "archive data error", NULL);
		}
		if (a1end > 0) {
			if (checklen)
				filediff(tc, "length differ", NULL);
			break;
		}

		/*
		 * Check member name if required.
		 */
		if (checkname) {
			if (strcmp(archive_entry_pathname(e1),
			    archive_entry_pathname(e2)) != 0)
				filediff(tc, "member name differ",
				    archive_entry_pathname(e1));
		}

		/*
		 * Compare time if required.
		 */
		if (checktime) {
			if (archive_entry_mtime(e1) !=
			    archive_entry_mtime(e2))
				filediff(tc, "member mtime differ",
				    archive_entry_pathname(e1));
		}

		/*
		 * Compare member size if required.
		 */
		if (checksize || checkcont) {
			size1 = (size_t) archive_entry_size(e1);
			size2 = (size_t) archive_entry_size(e2);
			if (size1 != size2)
				filediff(tc, "member size differ",
				    archive_entry_pathname(e1));
		}

		/*
		 * Compare member content if required.
		 */
		if (checkcont) {
			if ((buf1 = malloc(size1)) == NULL)
				filediff(tc, "not enough memory", NULL);
			if ((buf2 = malloc(size2)) == NULL)
				filediff(tc, "not enough memory", NULL);
			if ((size_t) archive_read_data(a1, buf1, size1) !=
			    size1)
				filediff(tc, "archive_read_data failed",
				    archive_entry_pathname(e1));
			if ((size_t) archive_read_data(a2, buf2, size2) !=
			    size2)
				filediff(tc, "archive_read_data failed",
				    archive_entry_pathname(e1));
			if (memcmp(buf1, buf2, size1) != 0)
				filediff(tc, "member content differ",
				    archive_entry_pathname(e1));
			free(buf1);
			free(buf2);
		}

		/* Proceed to next header. */
	}

	/* Passed! */
	filesame(tc);
	exit(EXIT_SUCCESS);
}

static void
filediff(const char *tc, const char *msg, const char *e)
{
	if (e != NULL)
		fprintf(stdout, "%s - archive diff not ok (%s (entry: %s))\n",
		    tc, msg, e);
	else
		fprintf(stdout, "%s - archive diff not ok (%s)\n", tc, msg);

	incct(COUNTER);
	exit(EXIT_SUCCESS);
}

static void
filesame(const char *tc)
{
	fprintf(stdout, "%s - archive diff ok\n", tc);
	incct(COUNTER);
	incct(PASSED);
}

static void
incct(const char *pathname)
{
	FILE *fp;
	char buf[10];

	if ((fp = fopen(pathname, "r")) != NULL) {
		if (fgets(buf, 10, fp) != buf)
			perror("fgets");
		snprintf(buf, 10, "%d\n", atoi(buf) + 1);
		fclose(fp);
	}
	if ((fp = fopen(pathname, "w")) != NULL) {
		fputs(buf, fp);
		fclose(fp);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: ardiff archive1 archive2\n");
	exit(EXIT_FAILURE);
}
