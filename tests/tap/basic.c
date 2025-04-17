/*
 * Some utility routines for writing tests.
 *
 * Here are a variety of utility routines for writing tests compatible with
 * the TAP protocol.  All routines of the form ok() or is*() take a test
 * number and some number of appropriate arguments, check to be sure the
 * results match the expected output using the arguments, and print out
 * something appropriate for that test number.  Other utility routines help in
 * constructing more complex tests, skipping tests, reporting errors, setting
 * up the TAP output format, or finding things in the test environment.
 *
 * This file is part of C TAP Harness.  The current version plus supporting
 * documentation is at <https://www.eyrie.org/~eagle/software/c-tap-harness/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009-2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2001-2002, 2004-2008, 2011-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#    include <direct.h>
#else
#    include <sys/stat.h>
#endif
#include <sys/types.h>
#include <unistd.h>

#include <tests/tap/basic.h>

/* Windows provides mkdir and rmdir under different names. */
#ifdef _WIN32
#    define mkdir(p, m) _mkdir(p)
#    define rmdir(p)    _rmdir(p)
#endif

/*
 * The test count.  Always contains the number that will be used for the next
 * test status.  This is exported to callers of the library.
 */
unsigned long testnum = 1;

/*
 * Status information stored so that we can give a test summary at the end of
 * the test case.  We store the planned final test and the count of failures.
 * We can get the highest test count from testnum.
 */
static unsigned long _planned = 0;
static unsigned long _failed = 0;

/*
 * Store the PID of the process that called plan() and only summarize
 * results when that process exits, so as to not misreport results in forked
 * processes.
 */
static pid_t _process = 0;

/*
 * If true, we're doing lazy planning and will print out the plan based on the
 * last test number at the end of testing.
 */
static int _lazy = 0;

/*
 * If true, the test was aborted by calling bail().  Currently, this is only
 * used to ensure that we pass a false value to any cleanup functions even if
 * all tests to that point have passed.
 */
static int _aborted = 0;

/*
 * Registered cleanup functions.  These are stored as a linked list and run in
 * registered order by finish when the test program exits.  Each function is
 * passed a boolean value indicating whether all tests were successful.
 */
struct cleanup_func {
    test_cleanup_func func;
    test_cleanup_func_with_data func_with_data;
    void *data;
    struct cleanup_func *next;
};
static struct cleanup_func *cleanup_funcs = NULL;

/*
 * Registered diag files.  Any output found in these files will be printed out
 * as if it were passed to diag() before any other output we do.  This allows
 * background processes to log to a file and have that output interleaved with
 * the test output.
 */
struct diag_file {
    char *name;
    FILE *file;
    char *buffer;
    size_t bufsize;
    struct diag_file *next;
};
static struct diag_file *diag_files = NULL;

/*
 * Print a specified prefix and then the test description.  Handles turning
 * the argument list into a va_args structure suitable for passing to
 * print_desc, which has to be done in a macro.  Assumes that format is the
 * argument immediately before the variadic arguments.
 */
#define PRINT_DESC(prefix, format)  \
    do {                            \
        if (format != NULL) {       \
            va_list args;           \
            printf("%s", prefix);   \
            va_start(args, format); \
            vprintf(format, args);  \
            va_end(args);           \
        }                           \
    } while (0)


/*
 * Form a new string by concatenating multiple strings.  The arguments must be
 * terminated by (const char *) 0.
 *
 * This function only exists because we can't assume asprintf.  We can't
 * simulate asprintf with snprintf because we're only assuming SUSv3, which
 * does not require that snprintf with a NULL buffer return the required
 * length.  When those constraints are relaxed, this should be ripped out and
 * replaced with asprintf or a more trivial replacement with snprintf.
 */
static char *
concat(const char *first, ...)
{
    va_list args;
    char *result;
    const char *string;
    size_t offset;
    size_t length = 0;

    /*
     * Find the total memory required.  Ensure we don't overflow length.  See
     * the comment for breallocarray for why we're using UINT_MAX here.
     */
    va_start(args, first);
    for (string = first; string != NULL; string = va_arg(args, const char *)) {
        if (length >= UINT_MAX - strlen(string))
            bail("strings too long in concat");
        length += strlen(string);
    }
    va_end(args);
    length++;

    /* Create the string. */
    result = bcalloc_type(length, char);
    va_start(args, first);
    offset = 0;
    for (string = first; string != NULL; string = va_arg(args, const char *)) {
        memcpy(result + offset, string, strlen(string));
        offset += strlen(string);
    }
    va_end(args);
    result[offset] = '\0';
    return result;
}


/*
 * Helper function for check_diag_files to handle a single line in a diag
 * file.
 *
 * The general scheme here used is as follows: read one line of output.  If we
 * get NULL, check for an error.  If there was one, bail out of the test
 * program; otherwise, return, and the enclosing loop will check for EOF.
 *
 * If we get some data, see if it ends in a newline.  If it doesn't end in a
 * newline, we have one of two cases: our buffer isn't large enough, in which
 * case we resize it and try again, or we have incomplete data in the file, in
 * which case we rewind the file and will try again next time.
 *
 * Returns a boolean indicating whether the last line was incomplete.
 */
static int
handle_diag_file_line(struct diag_file *file, fpos_t where)
{
    int size;
    size_t length;

    /* Read the next line from the file. */
    size = file->bufsize > INT_MAX ? INT_MAX : (int) file->bufsize;
    if (fgets(file->buffer, size, file->file) == NULL) {
        if (ferror(file->file))
            sysbail("cannot read from %s", file->name);
        return 0;
    }

    /*
     * See if the line ends in a newline.  If not, see which error case we
     * have.
     */
    length = strlen(file->buffer);
    if (file->buffer[length - 1] != '\n') {
        int incomplete = 0;

        /* Check whether we ran out of buffer space and resize if so. */
        if (length < file->bufsize - 1)
            incomplete = 1;
        else {
            file->bufsize += BUFSIZ;
            file->buffer =
                breallocarray_type(file->buffer, file->bufsize, char);
        }

        /*
         * On either incomplete lines or too small of a buffer, rewind
         * and read the file again (on the next pass, if incomplete).
         * It's simpler than trying to double-buffer the file.
         */
        if (fsetpos(file->file, &where) < 0)
            sysbail("cannot set position in %s", file->name);
        return incomplete;
    }

    /* We saw a complete line.  Print it out. */
    printf("# %s", file->buffer);
    return 0;
}


/*
 * Check all registered diag_files for any output.  We only print out the
 * output if we see a complete line; otherwise, we wait for the next newline.
 */
static void
check_diag_files(void)
{
    struct diag_file *file;
    fpos_t where;
    int incomplete;

    /*
     * Walk through each file and read each line of output available.
     */
    for (file = diag_files; file != NULL; file = file->next) {
        clearerr(file->file);

        /* Store the current position in case we have to rewind. */
        if (fgetpos(file->file, &where) < 0)
            sysbail("cannot get position in %s", file->name);

        /* Continue until we get EOF or an incomplete line of data. */
        incomplete = 0;
        while (!feof(file->file) && !incomplete) {
            incomplete = handle_diag_file_line(file, where);
        }
    }
}


/*
 * Our exit handler.  Called on completion of the test to report a summary of
 * results provided we're still in the original process.  This also handles
 * printing out the plan if we used plan_lazy(), although that's suppressed if
 * we never ran a test (due to an early bail, for example), and running any
 * registered cleanup functions.
 */
static void
finish(void)
{
    int success, primary;
    struct cleanup_func *current;
    unsigned long highest = testnum - 1;
    struct diag_file *file, *tmp;

    /* Check for pending diag_file output. */
    check_diag_files();

    /* Free the diag_files. */
    file = diag_files;
    while (file != NULL) {
        tmp = file;
        file = file->next;
        fclose(tmp->file);
        free(tmp->name);
        free(tmp->buffer);
        free(tmp);
    }
    diag_files = NULL;

    /*
     * Determine whether all tests were successful, which is needed before
     * calling cleanup functions since we pass that fact to the functions.
     */
    if (_planned == 0 && _lazy)
        _planned = highest;
    success = (!_aborted && _planned == highest && _failed == 0);

    /*
     * If there are any registered cleanup functions, we run those first.  We
     * always run them, even if we didn't run a test.  Don't do anything
     * except free the diag_files and call cleanup functions if we aren't the
     * primary process (the process in which plan or plan_lazy was called),
     * and tell the cleanup functions that fact.
     */
    primary = (_process == 0 || getpid() == _process);
    while (cleanup_funcs != NULL) {
        if (cleanup_funcs->func_with_data) {
            void *data = cleanup_funcs->data;

            cleanup_funcs->func_with_data(success, primary, data);
        } else {
            cleanup_funcs->func(success, primary);
        }
        current = cleanup_funcs;
        cleanup_funcs = cleanup_funcs->next;
        free(current);
    }
    if (!primary)
        return;

    /* Don't do anything further if we never planned a test. */
    if (_planned == 0)
        return;

    /* If we're aborting due to bail, don't print summaries. */
    if (_aborted)
        return;

    /* Print out the lazy plan if needed. */
    fflush(stderr);
    if (_lazy && _planned > 0)
        printf("1..%lu\n", _planned);

    /* Print out a summary of the results. */
    if (_planned > highest)
        diag("Looks like you planned %lu test%s but only ran %lu", _planned,
             (_planned > 1 ? "s" : ""), highest);
    else if (_planned < highest)
        diag("Looks like you planned %lu test%s but ran %lu extra", _planned,
             (_planned > 1 ? "s" : ""), highest - _planned);
    else if (_failed > 0)
        diag("Looks like you failed %lu test%s of %lu", _failed,
             (_failed > 1 ? "s" : ""), _planned);
    else if (_planned != 1)
        diag("All %lu tests successful or skipped", _planned);
    else
        diag("%lu test successful or skipped", _planned);
}


/*
 * Initialize things.  Turns on line buffering on stdout and then prints out
 * the number of tests in the test suite.  We intentionally don't check for
 * pending diag_file output here, since it should really come after the plan.
 */
void
plan(unsigned long count)
{
    if (setvbuf(stdout, NULL, _IOLBF, BUFSIZ) != 0)
        sysdiag("cannot set stdout to line buffered");
    fflush(stderr);
    printf("1..%lu\n", count);
    testnum = 1;
    _planned = count;
    _process = getpid();
    if (atexit(finish) != 0) {
        sysdiag("cannot register exit handler");
        diag("cleanups will not be run");
    }
}


/*
 * Initialize things for lazy planning, where we'll automatically print out a
 * plan at the end of the program.  Turns on line buffering on stdout as well.
 */
void
plan_lazy(void)
{
    if (setvbuf(stdout, NULL, _IOLBF, BUFSIZ) != 0)
        sysdiag("cannot set stdout to line buffered");
    testnum = 1;
    _process = getpid();
    _lazy = 1;
    if (atexit(finish) != 0)
        sysbail("cannot register exit handler to display plan");
}


/*
 * Skip the entire test suite and exits.  Should be called instead of plan(),
 * not after it, since it prints out a special plan line.  Ignore diag_file
 * output here, since it's not clear if it's allowed before the plan.
 */
void
skip_all(const char *format, ...)
{
    fflush(stderr);
    printf("1..0 # skip");
    PRINT_DESC(" ", format);
    putchar('\n');
    exit(0);
}


/*
 * Takes a boolean success value and assumes the test passes if that value
 * is true and fails if that value is false.
 */
int
ok(int success, const char *format, ...)
{
    fflush(stderr);
    check_diag_files();
    printf("%sok %lu", success ? "" : "not ", testnum++);
    if (!success)
        _failed++;
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Same as ok(), but takes the format arguments as a va_list.
 */
int
okv(int success, const char *format, va_list args)
{
    fflush(stderr);
    check_diag_files();
    printf("%sok %lu", success ? "" : "not ", testnum++);
    if (!success)
        _failed++;
    if (format != NULL) {
        printf(" - ");
        vprintf(format, args);
    }
    putchar('\n');
    return success;
}


/*
 * Skip a test.
 */
void
skip(const char *reason, ...)
{
    fflush(stderr);
    check_diag_files();
    printf("ok %lu # skip", testnum++);
    PRINT_DESC(" ", reason);
    putchar('\n');
}


/*
 * Report the same status on the next count tests.
 */
int
ok_block(unsigned long count, int success, const char *format, ...)
{
    unsigned long i;

    fflush(stderr);
    check_diag_files();
    for (i = 0; i < count; i++) {
        printf("%sok %lu", success ? "" : "not ", testnum++);
        if (!success)
            _failed++;
        PRINT_DESC(" - ", format);
        putchar('\n');
    }
    return success;
}


/*
 * Skip the next count tests.
 */
void
skip_block(unsigned long count, const char *reason, ...)
{
    unsigned long i;

    fflush(stderr);
    check_diag_files();
    for (i = 0; i < count; i++) {
        printf("ok %lu # skip", testnum++);
        PRINT_DESC(" ", reason);
        putchar('\n');
    }
}


/*
 * Takes two boolean values and requires the truth value of both match.
 */
int
is_bool(int left, int right, const char *format, ...)
{
    int success;

    fflush(stderr);
    check_diag_files();
    success = (!!left == !!right);
    if (success)
        printf("ok %lu", testnum++);
    else {
        diag(" left: %s", !!left ? "true" : "false");
        diag("right: %s", !!right ? "true" : "false");
        printf("not ok %lu", testnum++);
        _failed++;
    }
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Takes two integer values and requires they match.
 */
int
is_int(long left, long right, const char *format, ...)
{
    int success;

    fflush(stderr);
    check_diag_files();
    success = (left == right);
    if (success)
        printf("ok %lu", testnum++);
    else {
        diag(" left: %ld", left);
        diag("right: %ld", right);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Takes two strings and requires they match (using strcmp).  NULL arguments
 * are permitted and handled correctly.
 */
int
is_string(const char *left, const char *right, const char *format, ...)
{
    int success;

    fflush(stderr);
    check_diag_files();

    /* Compare the strings, being careful of NULL. */
    if (left == NULL)
        success = (right == NULL);
    else if (right == NULL)
        success = 0;
    else
        success = (strcmp(left, right) == 0);

    /* Report the results. */
    if (success)
        printf("ok %lu", testnum++);
    else {
        diag(" left: %s", left == NULL ? "(null)" : left);
        diag("right: %s", right == NULL ? "(null)" : right);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Takes two unsigned longs and requires they match.  On failure, reports them
 * in hex.
 */
int
is_hex(unsigned long left, unsigned long right, const char *format, ...)
{
    int success;

    fflush(stderr);
    check_diag_files();
    success = (left == right);
    if (success)
        printf("ok %lu", testnum++);
    else {
        diag(" left: %lx", (unsigned long) left);
        diag("right: %lx", (unsigned long) right);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Takes pointers to a regions of memory and requires that len bytes from each
 * match.  Otherwise reports any bytes which didn't match.
 */
int
is_blob(const void *left, const void *right, size_t len, const char *format,
        ...)
{
    int success;
    size_t i;

    fflush(stderr);
    check_diag_files();
    success = (memcmp(left, right, len) == 0);
    if (success)
        printf("ok %lu", testnum++);
    else {
        const unsigned char *left_c = (const unsigned char *) left;
        const unsigned char *right_c = (const unsigned char *) right;

        for (i = 0; i < len; i++) {
            if (left_c[i] != right_c[i])
                diag("offset %lu: left %02x, right %02x", (unsigned long) i,
                     left_c[i], right_c[i]);
        }
        printf("not ok %lu", testnum++);
        _failed++;
    }
    PRINT_DESC(" - ", format);
    putchar('\n');
    return success;
}


/*
 * Bail out with an error.
 */
void
bail(const char *format, ...)
{
    va_list args;

    _aborted = 1;
    fflush(stderr);
    check_diag_files();
    fflush(stdout);
    printf("Bail out! ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    exit(255);
}


/*
 * Bail out with an error, appending strerror(errno).
 */
void
sysbail(const char *format, ...)
{
    va_list args;
    int oerrno = errno;

    _aborted = 1;
    fflush(stderr);
    check_diag_files();
    fflush(stdout);
    printf("Bail out! ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(": %s\n", strerror(oerrno));
    exit(255);
}


/*
 * Report a diagnostic to stderr.  Always returns 1 to allow embedding in
 * compound statements.
 */
int
diag(const char *format, ...)
{
    va_list args;

    fflush(stderr);
    check_diag_files();
    fflush(stdout);
    printf("# ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    return 1;
}


/*
 * Report a diagnostic to stderr, appending strerror(errno).  Always returns 1
 * to allow embedding in compound statements.
 */
int
sysdiag(const char *format, ...)
{
    va_list args;
    int oerrno = errno;

    fflush(stderr);
    check_diag_files();
    fflush(stdout);
    printf("# ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(": %s\n", strerror(oerrno));
    return 1;
}


/*
 * Register a new file for diag_file processing.
 */
void
diag_file_add(const char *name)
{
    struct diag_file *file, *prev;

    file = bcalloc_type(1, struct diag_file);
    file->name = bstrdup(name);
    file->file = fopen(file->name, "r");
    if (file->file == NULL)
        sysbail("cannot open %s", name);
    file->buffer = bcalloc_type(BUFSIZ, char);
    file->bufsize = BUFSIZ;
    if (diag_files == NULL)
        diag_files = file;
    else {
        for (prev = diag_files; prev->next != NULL; prev = prev->next)
            ;
        prev->next = file;
    }
}


/*
 * Remove a file from diag_file processing.  If the file is not found, do
 * nothing, since there are some situations where it can be removed twice
 * (such as if it's removed from a cleanup function, since cleanup functions
 * are called after freeing all the diag_files).
 */
void
diag_file_remove(const char *name)
{
    struct diag_file *file;
    struct diag_file **prev = &diag_files;

    for (file = diag_files; file != NULL; file = file->next) {
        if (strcmp(file->name, name) == 0) {
            *prev = file->next;
            fclose(file->file);
            free(file->name);
            free(file->buffer);
            free(file);
            return;
        }
        prev = &file->next;
    }
}


/*
 * Allocate cleared memory, reporting a fatal error with bail on failure.
 */
void *
bcalloc(size_t n, size_t size)
{
    void *p;

    p = calloc(n, size);
    if (p == NULL)
        sysbail("failed to calloc %lu", (unsigned long) (n * size));
    return p;
}


/*
 * Allocate memory, reporting a fatal error with bail on failure.
 */
void *
bmalloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p == NULL)
        sysbail("failed to malloc %lu", (unsigned long) size);
    return p;
}


/*
 * Reallocate memory, reporting a fatal error with bail on failure.
 */
void *
brealloc(void *p, size_t size)
{
    p = realloc(p, size);
    if (p == NULL)
        sysbail("failed to realloc %lu bytes", (unsigned long) size);
    return p;
}


/*
 * The same as brealloc, but determine the size by multiplying an element
 * count by a size, similar to calloc.  The multiplication is checked for
 * integer overflow.
 *
 * We should technically use SIZE_MAX here for the overflow check, but
 * SIZE_MAX is C99 and we're only assuming C89 + SUSv3, which does not
 * guarantee that it exists.  They do guarantee that UINT_MAX exists, and we
 * can assume that UINT_MAX <= SIZE_MAX.
 *
 * (In theory, C89 and C99 permit size_t to be smaller than unsigned int, but
 * I disbelieve in the existence of such systems and they will have to cope
 * without overflow checks.)
 */
void *
breallocarray(void *p, size_t n, size_t size)
{
    if (n > 0 && UINT_MAX / n <= size)
        bail("reallocarray too large");
    if (n == 0)
        n = 1;
    p = realloc(p, n * size);
    if (p == NULL)
        sysbail("failed to realloc %lu bytes", (unsigned long) (n * size));
    return p;
}


/*
 * Copy a string, reporting a fatal error with bail on failure.
 */
char *
bstrdup(const char *s)
{
    char *p;
    size_t len;

    len = strlen(s) + 1;
    p = (char *) malloc(len);
    if (p == NULL)
        sysbail("failed to strdup %lu bytes", (unsigned long) len);
    memcpy(p, s, len);
    return p;
}


/*
 * Copy up to n characters of a string, reporting a fatal error with bail on
 * failure.  Don't use the system strndup function, since it may not exist and
 * the TAP library doesn't assume any portability support.
 */
char *
bstrndup(const char *s, size_t n)
{
    const char *p;
    char *copy;
    size_t length;

    /* Don't assume that the source string is nul-terminated. */
    for (p = s; (size_t)(p - s) < n && *p != '\0'; p++)
        ;
    length = (size_t)(p - s);
    copy = (char *) malloc(length + 1);
    if (copy == NULL)
        sysbail("failed to strndup %lu bytes", (unsigned long) length);
    memcpy(copy, s, length);
    copy[length] = '\0';
    return copy;
}


/*
 * Locate a test file.  Given the partial path to a file, look under
 * C_TAP_BUILD and then C_TAP_SOURCE for the file and return the full path to
 * the file.  Returns NULL if the file doesn't exist.  A non-NULL return
 * should be freed with test_file_path_free().
 */
char *
test_file_path(const char *file)
{
    char *base;
    char *path = NULL;
    const char *envs[] = {"C_TAP_BUILD", "C_TAP_SOURCE", NULL};
    int i;

    for (i = 0; envs[i] != NULL; i++) {
        base = getenv(envs[i]);
        if (base == NULL)
            continue;
        path = concat(base, "/", file, (const char *) 0);
        if (access(path, R_OK) == 0)
            break;
        free(path);
        path = NULL;
    }
    return path;
}


/*
 * Free a path returned from test_file_path().  This function exists primarily
 * for Windows, where memory must be freed from the same library domain that
 * it was allocated from.
 */
void
test_file_path_free(char *path)
{
    free(path);
}


/*
 * Create a temporary directory, tmp, under C_TAP_BUILD if set and the current
 * directory if it does not.  Returns the path to the temporary directory in
 * newly allocated memory, and calls bail on any failure.  The return value
 * should be freed with test_tmpdir_free.
 *
 * This function uses sprintf because it attempts to be independent of all
 * other portability layers.  The use immediately after a memory allocation
 * should be safe without using snprintf or strlcpy/strlcat.
 */
char *
test_tmpdir(void)
{
    const char *build;
    char *path = NULL;

    build = getenv("C_TAP_BUILD");
    if (build == NULL)
        build = ".";
    path = concat(build, "/tmp", (const char *) 0);
    if (access(path, X_OK) < 0)
        if (mkdir(path, 0777) < 0)
            sysbail("error creating temporary directory %s", path);
    return path;
}


/*
 * Free a path returned from test_tmpdir() and attempt to remove the
 * directory.  If we can't delete the directory, don't worry; something else
 * that hasn't yet cleaned up may still be using it.
 */
void
test_tmpdir_free(char *path)
{
    if (path != NULL)
        rmdir(path);
    free(path);
}

static void
register_cleanup(test_cleanup_func func,
                 test_cleanup_func_with_data func_with_data, void *data)
{
    struct cleanup_func *cleanup, **last;

    cleanup = bcalloc_type(1, struct cleanup_func);
    cleanup->func = func;
    cleanup->func_with_data = func_with_data;
    cleanup->data = data;
    cleanup->next = NULL;
    last = &cleanup_funcs;
    while (*last != NULL)
        last = &(*last)->next;
    *last = cleanup;
}

/*
 * Register a cleanup function that is called when testing ends.  All such
 * registered functions will be run by finish.
 */
void
test_cleanup_register(test_cleanup_func func)
{
    register_cleanup(func, NULL, NULL);
}

/*
 * Same as above, but also allows an opaque pointer to be passed to the cleanup
 * function.
 */
void
test_cleanup_register_with_data(test_cleanup_func_with_data func, void *data)
{
    register_cleanup(NULL, func, data);
}
