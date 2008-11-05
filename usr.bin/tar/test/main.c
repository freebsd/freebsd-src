/*
 * Copyright (c) 2003-2007 Tim Kientzle
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Various utility routines useful for test programs.
 * Each test program is linked against this file.
 */
#include "test.h"

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <time.h>

/*
 * This same file is used pretty much verbatim for all test harnesses.
 *
 * The next few lines are the only differences.
 */
#define	PROGRAM "bsdtar" /* Name of program being tested. */
#define ENVBASE "BSDTAR" /* Prefix for environment variables. */
#undef	EXTRA_DUMP	     /* How to dump extra data */
/* How to generate extra version info. */
#define	EXTRA_VERSION    (systemf("%s --version", testprog) ? "" : "")
__FBSDID("$FreeBSD$");

/*
 * "list.h" is simply created by "grep DEFINE_TEST"; it has
 * a line like
 *      DEFINE_TEST(test_function)
 * for each test.
 * Include it here with a suitable DEFINE_TEST to declare all of the
 * test functions.
 */
#undef DEFINE_TEST
#define	DEFINE_TEST(name) void name(void);
#include "list.h"

/* Interix doesn't define these in a standard header. */
#if __INTERIX__
extern char *optarg;
extern int optind;
#endif

/* Enable core dump on failure. */
static int dump_on_failure = 0;
/* Default is to remove temp dirs for successful tests. */
static int keep_temp_files = 0;
/* Default is to print some basic information about each test. */
static int quiet_flag = 0;
/* Default is to summarize repeated failures. */
static int verbose = 0;
/* Cumulative count of component failures. */
static int failures = 0;
/* Cumulative count of skipped component tests. */
static int skips = 0;
/* Cumulative count of assertions. */
static int assertions = 0;

/* Directory where uuencoded reference files can be found. */
static char *refdir;

/*
 * My own implementation of the standard assert() macro emits the
 * message in the same format as GCC (file:line: message).
 * It also includes some additional useful information.
 * This makes it a lot easier to skim through test failures in
 * Emacs.  ;-)
 *
 * It also supports a few special features specifically to simplify
 * test harnesses:
 *    failure(fmt, args) -- Stores a text string that gets
 *          printed if the following assertion fails, good for
 *          explaining subtle tests.
 */
static char msg[4096];

/*
 * For each test source file, we remember how many times each
 * failure was reported.
 */
static const char *failed_filename = NULL;
static struct line {
	int line;
	int count;
}  failed_lines[1000];

/*
 * Count this failure; return the number of previous failures.
 */
static int
previous_failures(const char *filename, int line)
{
	unsigned int i;
	int count;

	if (failed_filename == NULL || strcmp(failed_filename, filename) != 0)
		memset(failed_lines, 0, sizeof(failed_lines));
	failed_filename = filename;

	for (i = 0; i < sizeof(failed_lines)/sizeof(failed_lines[0]); i++) {
		if (failed_lines[i].line == line) {
			count = failed_lines[i].count;
			failed_lines[i].count++;
			return (count);
		}
		if (failed_lines[i].line == 0) {
			failed_lines[i].line = line;
			failed_lines[i].count = 1;
			return (0);
		}
	}
	return (0);
}

/*
 * Copy arguments into file-local variables.
 */
static const char *test_filename;
static int test_line;
static void *test_extra;
void test_setup(const char *filename, int line)
{
	test_filename = filename;
	test_line = line;
}

/*
 * Inform user that we're skipping a test.
 */
void
test_skipping(const char *fmt, ...)
{
	va_list ap;

	if (previous_failures(test_filename, test_line))
		return;

	va_start(ap, fmt);
	fprintf(stderr, " *** SKIPPING: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	++skips;
}

/* Common handling of failed tests. */
static void
report_failure(void *extra)
{
	if (msg[0] != '\0') {
		fprintf(stderr, "   Description: %s\n", msg);
		msg[0] = '\0';
	}

#ifdef EXTRA_DUMP
	if (extra != NULL)
		fprintf(stderr, "   detail: %s\n", EXTRA_DUMP(extra));
#else
	(void)extra; /* UNUSED */
#endif

	if (dump_on_failure) {
		fprintf(stderr,
		    " *** forcing core dump so failure can be debugged ***\n");
		*(char *)(NULL) = 0;
		exit(1);
	}
}

/*
 * Summarize repeated failures in the just-completed test file.
 * The reports above suppress multiple failures from the same source
 * line; this reports on any tests that did fail multiple times.
 */
static int
summarize_comparator(const void *a0, const void *b0)
{
	const struct line *a = a0, *b = b0;
	if (a->line == 0 && b->line == 0)
		return (0);
	if (a->line == 0)
		return (1);
	if (b->line == 0)
		return (-1);
	return (a->line - b->line);
}

static void
summarize(void)
{
	unsigned int i;

	qsort(failed_lines, sizeof(failed_lines)/sizeof(failed_lines[0]),
	    sizeof(failed_lines[0]), summarize_comparator);
	for (i = 0; i < sizeof(failed_lines)/sizeof(failed_lines[0]); i++) {
		if (failed_lines[i].line == 0)
			break;
		if (failed_lines[i].count > 1)
			fprintf(stderr, "%s:%d: Failed %d times\n",
			    failed_filename, failed_lines[i].line,
			    failed_lines[i].count);
	}
	/* Clear the failure history for the next file. */
	memset(failed_lines, 0, sizeof(failed_lines));
}

/* Set up a message to display only after a test fails. */
void
failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);
	va_end(ap);
}

/* Generic assert() just displays the failed condition. */
int
test_assert(const char *file, int line, int value, const char *condition, void *extra)
{
	++assertions;
	if (value) {
		msg[0] = '\0';
		return (value);
	}
	failures ++;
	if (!verbose && previous_failures(file, line))
		return (value);
	fprintf(stderr, "%s:%d: Assertion failed\n", file, line);
	fprintf(stderr, "   Condition: %s\n", condition);
	report_failure(extra);
	return (value);
}

/* assertEqualInt() displays the values of the two integers. */
int
test_assert_equal_int(const char *file, int line,
    int v1, const char *e1, int v2, const char *e2, void *extra)
{
	++assertions;
	if (v1 == v2) {
		msg[0] = '\0';
		return (1);
	}
	failures ++;
	if (!verbose && previous_failures(file, line))
		return (0);
	fprintf(stderr, "%s:%d: Assertion failed: Ints not equal\n",
	    file, line);
	fprintf(stderr, "      %s=%d\n", e1, v1);
	fprintf(stderr, "      %s=%d\n", e2, v2);
	report_failure(extra);
	return (0);
}

static void strdump(const char *p)
{
	if (p == NULL) {
		fprintf(stderr, "(null)");
		return;
	}
	fprintf(stderr, "\"");
	while (*p != '\0') {
		unsigned int c = 0xff & *p++;
		switch (c) {
		case '\a': fprintf(stderr, "\a"); break;
		case '\b': fprintf(stderr, "\b"); break;
		case '\n': fprintf(stderr, "\n"); break;
		case '\r': fprintf(stderr, "\r"); break;
		default:
			if (c >= 32 && c < 127)
				fprintf(stderr, "%c", c);
			else
				fprintf(stderr, "\\x%02X", c);
		}
	}
	fprintf(stderr, "\"");
}

/* assertEqualString() displays the values of the two strings. */
int
test_assert_equal_string(const char *file, int line,
    const char *v1, const char *e1,
    const char *v2, const char *e2,
    void *extra)
{
	++assertions;
	if (v1 == NULL || v2 == NULL) {
		if (v1 == v2) {
			msg[0] = '\0';
			return (1);
		}
	} else if (strcmp(v1, v2) == 0) {
		msg[0] = '\0';
		return (1);
	}
	failures ++;
	if (!verbose && previous_failures(file, line))
		return (0);
	fprintf(stderr, "%s:%d: Assertion failed: Strings not equal\n",
	    file, line);
	fprintf(stderr, "      %s = ", e1);
	strdump(v1);
	fprintf(stderr, " (length %d)\n", v1 == NULL ? 0 : (int)strlen(v1));
	fprintf(stderr, "      %s = ", e2);
	strdump(v2);
	fprintf(stderr, " (length %d)\n", v2 == NULL ? 0 : (int)strlen(v2));
	report_failure(extra);
	return (0);
}

static void wcsdump(const wchar_t *w)
{
	if (w == NULL) {
		fprintf(stderr, "(null)");
		return;
	}
	fprintf(stderr, "\"");
	while (*w != L'\0') {
		unsigned int c = *w++;
		if (c >= 32 && c < 127)
			fprintf(stderr, "%c", c);
		else if (c < 256)
			fprintf(stderr, "\\x%02X", c);
		else if (c < 0x10000)
			fprintf(stderr, "\\u%04X", c);
		else
			fprintf(stderr, "\\U%08X", c);
	}
	fprintf(stderr, "\"");
}

/* assertEqualWString() displays the values of the two strings. */
int
test_assert_equal_wstring(const char *file, int line,
    const wchar_t *v1, const char *e1,
    const wchar_t *v2, const char *e2,
    void *extra)
{
	++assertions;
	if (v1 == NULL) {
		if (v2 == NULL) {
			msg[0] = '\0';
			return (1);
		}
	} else if (v2 == NULL) {
		if (v1 == NULL) {
			msg[0] = '\0';
			return (1);
		}
	} else if (wcscmp(v1, v2) == 0) {
		msg[0] = '\0';
		return (1);
	}
	failures ++;
	if (!verbose && previous_failures(file, line))
		return (0);
	fprintf(stderr, "%s:%d: Assertion failed: Unicode strings not equal\n",
	    file, line);
	fprintf(stderr, "      %s = ", e1);
	wcsdump(v1);
	fprintf(stderr, "\n");
	fprintf(stderr, "      %s = ", e2);
	wcsdump(v2);
	fprintf(stderr, "\n");
	report_failure(extra);
	return (0);
}

/*
 * Pretty standard hexdump routine.  As a bonus, if ref != NULL, then
 * any bytes in p that differ from ref will be highlighted with '_'
 * before and after the hex value.
 */
static void
hexdump(const char *p, const char *ref, size_t l, size_t offset)
{
	size_t i, j;
	char sep;

	for(i=0; i < l; i+=16) {
		fprintf(stderr, "%04x", (int)(i + offset));
		sep = ' ';
		for (j = 0; j < 16 && i + j < l; j++) {
			if (ref != NULL && p[i + j] != ref[i + j])
				sep = '_';
			fprintf(stderr, "%c%02x", sep, 0xff & (int)p[i+j]);
			if (ref != NULL && p[i + j] == ref[i + j])
				sep = ' ';
		}
		for (; j < 16; j++) {
			fprintf(stderr, "%c  ", sep);
			sep = ' ';
		}
		fprintf(stderr, "%c", sep);
		for (j=0; j < 16 && i + j < l; j++) {
			int c = p[i + j];
			if (c >= ' ' && c <= 126)
				fprintf(stderr, "%c", c);
			else
				fprintf(stderr, ".");
		}
		fprintf(stderr, "\n");
	}
}

/* assertEqualMem() displays the values of the two memory blocks. */
/* TODO: For long blocks, hexdump the first bytes that actually differ. */
int
test_assert_equal_mem(const char *file, int line,
    const char *v1, const char *e1,
    const char *v2, const char *e2,
    size_t l, const char *ld, void *extra)
{
	++assertions;
	if (v1 == NULL || v2 == NULL) {
		if (v1 == v2) {
			msg[0] = '\0';
			return (1);
		}
	} else if (memcmp(v1, v2, l) == 0) {
		msg[0] = '\0';
		return (1);
	}
	failures ++;
	if (!verbose && previous_failures(file, line))
		return (0);
	fprintf(stderr, "%s:%d: Assertion failed: memory not equal\n",
	    file, line);
	fprintf(stderr, "      size %s = %d\n", ld, (int)l);
	fprintf(stderr, "      Dump of %s\n", e1);
	hexdump(v1, v2, l < 32 ? l : 32, 0);
	fprintf(stderr, "      Dump of %s\n", e2);
	hexdump(v2, v1, l < 32 ? l : 32, 0);
	fprintf(stderr, "\n");
	report_failure(extra);
	return (0);
}

int
test_assert_empty_file(const char *f1fmt, ...)
{
	char buff[1024];
	char f1[1024];
	struct stat st;
	va_list ap;
	ssize_t s;
	int fd;


	va_start(ap, f1fmt);
	vsprintf(f1, f1fmt, ap);
	va_end(ap);

	if (stat(f1, &st) != 0) {
		fprintf(stderr, "%s:%d: Could not stat: %s\n", test_filename, test_line, f1);
		report_failure(NULL);
		return (0);
	}
	if (st.st_size == 0)
		return (1);

	failures ++;
	if (!verbose && previous_failures(test_filename, test_line))
		return (0);

	fprintf(stderr, "%s:%d: File not empty: %s\n", test_filename, test_line, f1);
	fprintf(stderr, "    File size: %d\n", (int)st.st_size);
	fprintf(stderr, "    Contents:\n");
	fd = open(f1, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "    Unable to open %s\n", f1);
	} else {
		s = (sizeof(buff) < (size_t)st.st_size) ?
		    (ssize_t)sizeof(buff) : (ssize_t)st.st_size;
		s = read(fd, buff, s);
		hexdump(buff, NULL, s, 0);
	}
	report_failure(NULL);
	return (0);
}

int
test_assert_non_empty_file(const char *f1fmt, ...)
{
	char f1[1024];
	struct stat st;
	va_list ap;


	va_start(ap, f1fmt);
	vsprintf(f1, f1fmt, ap);
	va_end(ap);

	if (stat(f1, &st) != 0) {
		fprintf(stderr, "%s:%d: Could not stat: %s\n",
		    test_filename, test_line, f1);
		report_failure(NULL);
		return (0);
	}
	if (st.st_size != 0)
		return (1);

	failures ++;
	if (!verbose && previous_failures(test_filename, test_line))
		return (0);

	fprintf(stderr, "%s:%d: File empty: %s\n",
	    test_filename, test_line, f1);
	report_failure(NULL);
	return (0);
}

/* assertEqualFile() asserts that two files have the same contents. */
/* TODO: hexdump the first bytes that actually differ. */
int
test_assert_equal_file(const char *f1, const char *f2pattern, ...)
{
	char f2[1024];
	va_list ap;
	char buff1[1024];
	char buff2[1024];
	int fd1, fd2;
	int n1, n2;

	va_start(ap, f2pattern);
	vsprintf(f2, f2pattern, ap);
	va_end(ap);

	fd1 = open(f1, O_RDONLY);
	fd2 = open(f2, O_RDONLY);
	for (;;) {
		n1 = read(fd1, buff1, sizeof(buff1));
		n2 = read(fd2, buff2, sizeof(buff2));
		if (n1 != n2)
			break;
		if (n1 == 0 && n2 == 0)
			return (1);
		if (memcmp(buff1, buff2, n1) != 0)
			break;
	}
	failures ++;
	if (!verbose && previous_failures(test_filename, test_line))
		return (0);
	fprintf(stderr, "%s:%d: Files are not identical\n",
	    test_filename, test_line);
	fprintf(stderr, "  file1=\"%s\"\n", f1);
	fprintf(stderr, "  file2=\"%s\"\n", f2);
	report_failure(test_extra);
	return (0);
}

int
test_assert_file_exists(const char *fpattern, ...)
{
	char f[1024];
	va_list ap;

	va_start(ap, fpattern);
	vsprintf(f, fpattern, ap);
	va_end(ap);

	if (!access(f, F_OK))
		return (1);
	if (!previous_failures(test_filename, test_line)) {
		fprintf(stderr, "%s:%d: File doesn't exist\n",
		    test_filename, test_line);
		fprintf(stderr, "  file=\"%s\"\n", f);
		report_failure(test_extra);
	}
	return (0);
}

int
test_assert_file_not_exists(const char *fpattern, ...)
{
	char f[1024];
	va_list ap;

	va_start(ap, fpattern);
	vsprintf(f, fpattern, ap);
	va_end(ap);

	if (access(f, F_OK))
		return (1);
	if (!previous_failures(test_filename, test_line)) {
		fprintf(stderr, "%s:%d: File exists and shouldn't\n",
		    test_filename, test_line);
		fprintf(stderr, "  file=\"%s\"\n", f);
		report_failure(test_extra);
	}
	return (0);
}

/* assertFileContents() asserts the contents of a file. */
int
test_assert_file_contents(const void *buff, int s, const char *fpattern, ...)
{
	char f[1024];
	va_list ap;
	char *contents;
	int fd;
	int n;

	va_start(ap, fpattern);
	vsprintf(f, fpattern, ap);
	va_end(ap);

	fd = open(f, O_RDONLY);
	contents = malloc(s * 2);
	n = read(fd, contents, s * 2);
	if (n == s && memcmp(buff, contents, s) == 0) {
		free(contents);
		return (1);
	}
	failures ++;
	if (!previous_failures(test_filename, test_line)) {
		fprintf(stderr, "%s:%d: File contents don't match\n",
		    test_filename, test_line);
		fprintf(stderr, "  file=\"%s\"\n", f);
		if (n > 0)
			hexdump(contents, buff, n, 0);
		else {
			fprintf(stderr, "  File empty, contents should be:\n");
			hexdump(buff, NULL, s, 0);
		}
		report_failure(test_extra);
	}
	free(contents);
	return (0);
}

/*
 * Call standard system() call, but build up the command line using
 * sprintf() conventions.
 */
int
systemf(const char *fmt, ...)
{
	char buff[8192];
	va_list ap;
	int r;

	va_start(ap, fmt);
	vsprintf(buff, fmt, ap);
	r = system(buff);
	va_end(ap);
	return (r);
}

/*
 * Slurp a file into memory for ease of comparison and testing.
 * Returns size of file in 'sizep' if non-NULL, null-terminates
 * data in memory for ease of use.
 */
char *
slurpfile(size_t * sizep, const char *fmt, ...)
{
	char filename[8192];
	struct stat st;
	va_list ap;
	char *p;
	ssize_t bytes_read;
	int fd;
	int r;

	va_start(ap, fmt);
	vsprintf(filename, fmt, ap);
	va_end(ap);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		/* Note: No error; non-existent file is okay here. */
		return (NULL);
	}
	r = fstat(fd, &st);
	if (r != 0) {
		fprintf(stderr, "Can't stat file %s\n", filename);
		close(fd);
		return (NULL);
	}
	p = malloc(st.st_size + 1);
	if (p == NULL) {
		fprintf(stderr, "Can't allocate %ld bytes of memory to read file %s\n", (long int)st.st_size, filename);
		close(fd);
		return (NULL);
	}
	bytes_read = read(fd, p, st.st_size);
	if (bytes_read < st.st_size) {
		fprintf(stderr, "Can't read file %s\n", filename);
		close(fd);
		free(p);
		return (NULL);
	}
	p[st.st_size] = '\0';
	if (sizep != NULL)
		*sizep = (size_t)st.st_size;
	close(fd);
	return (p);
}

/*
 * "list.h" is automatically generated; it just has a lot of lines like:
 * 	DEFINE_TEST(function_name)
 * It's used above to declare all of the test functions.
 * We reuse it here to define a list of all tests (functions and names).
 */
#undef DEFINE_TEST
#define	DEFINE_TEST(n) { n, #n },
struct { void (*func)(void); const char *name; } tests[] = {
	#include "list.h"
};

/*
 * Each test is run in a private work dir.  Those work dirs
 * do have consistent and predictable names, in case a group
 * of tests need to collaborate.  However, there is no provision
 * for requiring that tests run in a certain order.
 */
static int test_run(int i, const char *tmpdir)
{
	int failures_before = failures;

	if (!quiet_flag) {
		printf("%d: %s\n", i, tests[i].name);
		fflush(stdout);
	}

	/*
	 * Always explicitly chdir() in case the last test moved us to
	 * a strange place.
	 */
	if (chdir(tmpdir)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir %s\n",
		    tmpdir);
		exit(1);
	}
	/* Create a temp directory for this specific test. */
	if (mkdir(tests[i].name, 0755)) {
		fprintf(stderr,
		    "ERROR: Couldn't create temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	/* Chdir() to that work directory. */
	if (chdir(tests[i].name)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	/* Explicitly reset the locale before each test. */
	setlocale(LC_ALL, "C");
	/* Run the actual test. */
	(*tests[i].func)();
	/* Summarize the results of this test. */
	summarize();
	/* If there were no failures, we can remove the work dir. */
	if (failures == failures_before) {
		if (!keep_temp_files && chdir(tmpdir) == 0) {
			systemf("rm -rf %s", tests[i].name);
		}
	}
	/* Return appropriate status. */
	return (failures == failures_before ? 0 : 1);
}

static void usage(const char *program)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i;

	printf("Usage: %s [options] <test> <test> ...\n", program);
	printf("Default is to run all tests.\n");
	printf("Otherwise, specify the numbers of the tests you wish to run.\n");
	printf("Options:\n");
	printf("  -d  Dump core after any failure, for debugging.\n");
	printf("  -k  Keep all temp files.\n");
	printf("      Default: temp files for successful tests deleted.\n");
#ifdef PROGRAM
	printf("  -p <path>  Path to executable to be tested.\n");
	printf("      Default: path taken from " ENVBASE " environment variable.\n");
#endif
	printf("  -q  Quiet.\n");
	printf("  -r <dir>   Path to dir containing reference files.\n");
	printf("      Default: Current directory.\n");
	printf("  -v  Verbose.\n");
	printf("Available tests:\n");
	for (i = 0; i < limit; i++)
		printf("  %d: %s\n", i, tests[i].name);
	exit(1);
}

#define	UUDECODE(c) (((c) - 0x20) & 0x3f)

void
extract_reference_file(const char *name)
{
	char buff[1024];
	FILE *in, *out;

	sprintf(buff, "%s/%s.uu", refdir, name);
	in = fopen(buff, "r");
	failure("Couldn't open reference file %s", buff);
	assert(in != NULL);
	if (in == NULL)
		return;
	/* Read up to and including the 'begin' line. */
	for (;;) {
		if (fgets(buff, sizeof(buff), in) == NULL) {
			/* TODO: This is a failure. */
			return;
		}
		if (memcmp(buff, "begin ", 6) == 0)
			break;
	}
	/* Now, decode the rest and write it. */
	/* Not a lot of error checking here; the input better be right. */
	out = fopen(name, "w");
	while (fgets(buff, sizeof(buff), in) != NULL) {
		char *p = buff;
		int bytes;

		if (memcmp(buff, "end", 3) == 0)
			break;

		bytes = UUDECODE(*p++);
		while (bytes > 0) {
			int n = 0;
			/* Write out 1-3 bytes from that. */
			if (bytes > 0) {
				n = UUDECODE(*p++) << 18;
				n |= UUDECODE(*p++) << 12;
				fputc(n >> 16, out);
				--bytes;
			}
			if (bytes > 0) {
				n |= UUDECODE(*p++) << 6;
				fputc((n >> 8) & 0xFF, out);
				--bytes;
			}
			if (bytes > 0) {
				n |= UUDECODE(*p++);
				fputc(n & 0xFF, out);
				--bytes;
			}
		}
	}
	fclose(out);
	fclose(in);
}


int main(int argc, char **argv)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i, tests_run = 0, tests_failed = 0, opt;
	time_t now;
	char *refdir_alloc = NULL;
	char *progname, *p;
	char tmpdir[256];
	char tmpdir_timestamp[256];

	/*
	 * Name of this program, used to build root of our temp directory
	 * tree.
	 */
	progname = p = argv[0];
	while (*p != '\0') {
		if (*p == '/')
			progname = p + 1;
		++p;
	}

#ifdef PROGRAM
	/* Get the target program from environment, if available. */
	testprog = getenv(ENVBASE);
#endif

	/* Allow -d to be controlled through the environment. */
	if (getenv(ENVBASE "_DEBUG") != NULL)
		dump_on_failure = 1;

	/* Get the directory holding test files from environment. */
	refdir = getenv(ENVBASE "_TEST_FILES");

	/*
	 * Parse options.
	 */
	while ((opt = getopt(argc, argv, "dkp:qr:v")) != -1) {
		switch (opt) {
		case 'd':
			dump_on_failure = 1;
			break;
		case 'k':
			keep_temp_files = 1;
			break;
		case 'p':
#ifdef PROGRAM
			testprog = optarg;
#else
			usage(progname);
#endif
			break;
		case 'q':
			quiet_flag++;
			break;
		case 'r':
			refdir = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * Sanity-check that our options make sense.
	 */
#ifdef PROGRAM
	if (testprog == NULL)
		usage(progname);
#endif

	/*
	 * Create a temp directory for the following tests.
	 * Include the time the tests started as part of the name,
	 * to make it easier to track the results of multiple tests.
	 */
	now = time(NULL);
	for (i = 0; i < 1000; i++) {
		strftime(tmpdir_timestamp, sizeof(tmpdir_timestamp),
		    "%Y-%m-%dT%H.%M.%S",
		    localtime(&now));
		sprintf(tmpdir, "/tmp/%s.%s-%03d", progname, tmpdir_timestamp, i);
		if (mkdir(tmpdir,0755) == 0)
			break;
		if (errno == EEXIST)
			continue;
		fprintf(stderr, "ERROR: Unable to create temp directory %s\n",
		    tmpdir);
		exit(1);
	}

	/*
	 * If the user didn't specify a directory for locating
	 * reference files, use the current directory for that.
	 */
	if (refdir == NULL) {
		systemf("/bin/pwd > %s/refdir", tmpdir);
		refdir = refdir_alloc = slurpfile(NULL, "%s/refdir", tmpdir);
		p = refdir + strlen(refdir);
		while (p[-1] == '\n') {
			--p;
			*p = '\0';
		}
		systemf("rm %s/refdir", tmpdir);
	}

	/*
	 * Banner with basic information.
	 */
	if (!quiet_flag) {
		printf("Running tests in: %s\n", tmpdir);
		printf("Reference files will be read from: %s\n", refdir);
#ifdef PROGRAM
		printf("Running tests on: %s\n", testprog);
#endif
		printf("Exercising: ");
		fflush(stdout);
		printf("%s\n", EXTRA_VERSION);
	}

	/*
	 * Run some or all of the individual tests.
	 */
	if (argc == 0) {
		/* Default: Run all tests. */
		for (i = 0; i < limit; i++) {
			if (test_run(i, tmpdir))
				tests_failed++;
			tests_run++;
		}
	} else {
		while (*(argv) != NULL) {
			i = atoi(*argv);
			if (**argv < '0' || **argv > '9' || i < 0 || i >= limit) {
				printf("*** INVALID Test %s\n", *argv);
				usage(progname);
			} else {
				if (test_run(i, tmpdir))
					tests_failed++;
				tests_run++;
			}
			argv++;
		}
	}

	/*
	 * Report summary statistics.
	 */
	if (!quiet_flag) {
		printf("\n");
		printf("%d of %d tests reported failures\n",
		    tests_failed, tests_run);
		printf(" Total of %d assertions checked.\n", assertions);
		printf(" Total of %d assertions failed.\n", failures);
		printf(" Total of %d assertions skipped.\n", skips);
	}

	free(refdir_alloc);

	/* If the final tmpdir is empty, we can remove it. */
	/* This should be the usual case when all tests succeed. */
	rmdir(tmpdir);

	return (tests_failed);
}
