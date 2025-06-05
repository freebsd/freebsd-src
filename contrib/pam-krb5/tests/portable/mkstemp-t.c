/*
 * mkstemp test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <config.h>
#include <portable/system.h>

#include <errno.h>
#include <sys/stat.h>

#include <tests/tap/basic.h>

int test_mkstemp(char *template);

int
main(void)
{
    int fd;
    char template[] = "tsXXXXXXX";
    char tooshort[] = "XXXXX";
    char bad1[] = "/foo/barXXXXX";
    char bad2[] = "/foo/barXXXXXX.out";
    char buffer[256];
    struct stat st1, st2;
    ssize_t length;

    plan(20);

    /* First, test a few error messages. */
    errno = 0;
    is_int(-1, test_mkstemp(tooshort), "too short of template");
    is_int(EINVAL, errno, "...with correct errno");
    is_string("XXXXX", tooshort, "...and template didn't change");
    errno = 0;
    is_int(-1, test_mkstemp(bad1), "bad template");
    is_int(EINVAL, errno, "...with correct errno");
    is_string("/foo/barXXXXX", bad1, "...and template didn't change");
    errno = 0;
    is_int(-1, test_mkstemp(bad2), "template doesn't end in XXXXXX");
    is_int(EINVAL, errno, "...with correct errno");
    is_string("/foo/barXXXXXX.out", bad2, "...and template didn't change");
    errno = 0;

    /* Now try creating a real file. */
    fd = test_mkstemp(template);
    ok(fd >= 0, "mkstemp works with valid template");
    ok(strcmp(template, "tsXXXXXXX") != 0, "...and template changed");
    ok(strncmp(template, "tsX", 3) == 0, "...and didn't touch first X");
    ok(access(template, F_OK) == 0, "...and the file exists");

    /* Make sure that it's the same file as template refers to now. */
    ok(stat(template, &st1) == 0, "...and stat of template works");
    ok(fstat(fd, &st2) == 0, "...and stat of open file descriptor works");
    ok(st1.st_ino == st2.st_ino, "...and they're the same file");
    unlink(template);

    /* Make sure the open mode is correct. */
    length = strlen(template);
    is_int(length, write(fd, template, length), "write to open file works");
    ok(lseek(fd, 0, SEEK_SET) == 0, "...and rewind works");
    is_int(length, read(fd, buffer, length), "...and the data is there");
    buffer[length] = '\0';
    is_string(template, buffer, "...and matches what we wrote");
    close(fd);

    return 0;
}
