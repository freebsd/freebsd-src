/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/profile/t_profile.c - profile library regression tests */
/*
 * Copyright (C) 2021 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "profile.h"

static void
check(long code)
{
    assert(code == 0);
}

static void
check_fail(long code, long expected)
{
    assert(code == expected);
}

static void
write_file(const char *name, int nlines, ...)
{
    FILE *f;
    va_list ap;
    int i;

    (void)unlink(name);
    f = fopen(name, "w");
    assert(f != NULL);
    va_start(ap, nlines);
    for (i = 0; i < nlines; i++)
        fprintf(f, "%s\n", va_arg(ap, char *));
    va_end(ap);
    fclose(f);
}

/* Regression test for #2685 (profile iterator breaks when modifications
 * made) */
static void
test_iterate()
{
    profile_t p;
    void *iter;
    const char *names[] = { "test section 1", "child_section", "child", NULL };
    const char *values[] = { "slick", "harry", "john", NULL };
    char *name, *value;
    int i;

    check(profile_init_path("test2.ini", &p));

    /* Iterate and check for the expected values. */
    check(profile_iterator_create(p, names, 0, &iter));
    for (i = 0;; i++) {
        check(profile_iterator(&iter, &name, &value));
        if (name == NULL && value == NULL)
            break;
        assert(strcmp(name, names[2]) == 0);
        assert(values[i] != NULL);
        assert(strcmp(value, values[i]) == 0);
        profile_release_string(name);
        profile_release_string(value);
    }
    assert(values[i] == NULL);
    profile_iterator_free(&iter);

    /* Iterate again, deleting each value as we go.  Flush the result to a
     * separate file. */
    check(profile_iterator_create(p, names, 0, &iter));
    for (;;) {
        check(profile_iterator(&iter, NULL, &value));
        if (value == NULL)
            break;
        check(profile_update_relation(p, names, value, NULL));
        profile_release_string(value);
    }
    profile_iterator_free(&iter);
    (void)unlink("test3.ini");
    profile_flush_to_file(p, "test3.ini");

    profile_abandon(p);

    /* Check that no values for the section are found in the resulting file. */
    check(profile_init_path("test3.ini", &p));
    check(profile_iterator_create(p, names, 0, &iter));
    check(profile_iterator(&iter, &name, &value));
    assert(name == NULL && value == NULL);
    profile_iterator_free(&iter);
    profile_abandon(p);
}

/*
 * Regression test for a 1.4-era bug where updating the underlying file data of
 * a profile object lost track of the flag indicating that it was part of the
 * global shared profiles list.
 */
static void
test_shared()
{
    profile_t a, b;
    struct utimbuf times;

    system("cp test2.ini test3.ini");

    /* Create an entry in the shared table. */
    check(profile_init_path("test3.ini", &a));

    /*
     * Force an update of the underlying data.  With the bug present, the
     * shared flag is erroneously cleared.  The easiest way to force an update
     * is to reopen the file (since we don't enforce the one-stat-per-second
     * limit during open) after changing the timestamp.
     */
    times.actime = time(NULL) + 2;
    times.modtime = times.actime;
    utime("test3.ini", &times);
    check(profile_init_path("test3.ini", &b));
    profile_release(b);

    /* Release the profile.  With the bug present, a dangling reference is left
     * behind in the shared table. */
    profile_release(a);

    /* Open the profile again to dereference the dangling pointer if one was
     * created. */
    check(profile_init_path("test3.ini", &a));
    profile_release(a);
}

/* Regression test for #2950 (profile_clear_relation not reflected within
 * handle where deletion is performed) */
static void
test_clear()
{
    profile_t p;
    const char *names[] = { "test section 1", "quux", NULL };
    char **values, **dummy;

    check(profile_init_path("test2.ini", &p));
    check(profile_get_values(p, names, &values));
    check(profile_clear_relation(p, names));
    check_fail(profile_get_values(p, names, &dummy), PROF_NO_RELATION);
    check(profile_add_relation(p, names, values[0]));
    profile_free_list(values);
    check(profile_get_values(p, names, &values));
    assert(values[0] != NULL && values[1] == NULL);
    profile_free_list(values);
    profile_abandon(p);
}

static void
test_include()
{
    profile_t p;
    const char *names[] = { "test section 1", "bar", NULL };
    char **values;

    /* Test expected error code when including nonexistent file. */
    write_file("testinc.ini", 1, "include does-not-exist");
    check_fail(profile_init_path("testinc.ini", &p), PROF_FAIL_INCLUDE_FILE);

    /* Test expected error code when including nonexistent directory. */
    write_file("testinc.ini", 1, "includedir does-not-exist");
    check_fail(profile_init_path("testinc.ini", &p), PROF_FAIL_INCLUDE_DIR);

    /* Test including a file. */
    write_file("testinc.ini", 1, "include test2.ini");
    check(profile_init_path("testinc.ini", &p));
    check(profile_get_values(p, names, &values));
    assert(strcmp(values[0], "foo") == 0 && values[1] == NULL);
    profile_free_list(values);
    profile_release(p);

    /*
     * Test including a directory.  Put four copies of test2.ini inside the
     * directory, two with invalid names.  Check that we get two values for one
     * of the variables.
     */
    system("rm -rf test_include_dir");
    system("mkdir test_include_dir");
    system("cp test2.ini test_include_dir/a");
    system("cp test2.ini test_include_dir/a~");
    system("cp test2.ini test_include_dir/b.conf");
    system("cp test2.ini test_include_dir/b.conf.rpmsave");
    write_file("testinc.ini", 1, "includedir test_include_dir");
    check(profile_init_path("testinc.ini", &p));
    check(profile_get_values(p, names, &values));
    assert(strcmp(values[0], "foo") == 0);
    assert(strcmp(values[1], "foo") == 0);
    assert(values[2] == NULL);
    profile_free_list(values);
    profile_release(p);

    /* Directly list the directory in the profile path and try again. */
    check(profile_init_path("test_include_dir", &p));
    check(profile_get_values(p, names, &values));
    assert(strcmp(values[0], "foo") == 0);
    assert(strcmp(values[1], "foo") == 0);
    assert(values[2] == NULL);
    profile_free_list(values);
    profile_release(p);
}

/* Test syntactic independence of included profile files. */
static void
test_independence()
{
    profile_t p;
    const char *names1[] = { "sec1", "var", "a", NULL };
    const char *names2[] = { "sec2", "b", NULL };
    const char *names3[] = { "sec1", "var", "c", NULL };
    char **values;

    write_file("testinc.ini", 6, "[sec1]", "var = {", "a = 1",
               "include testinc2.ini", "c = 3", "}");
    write_file("testinc2.ini", 2, "[sec2]", "b = 2");

    check(profile_init_path("testinc.ini", &p));
    check(profile_get_values(p, names1, &values));
    assert(strcmp(values[0], "1") == 0 && values[1] == NULL);
    profile_free_list(values);
    check(profile_get_values(p, names2, &values));
    assert(strcmp(values[0], "2") == 0 && values[1] == NULL);
    profile_free_list(values);
    check(profile_get_values(p, names3, &values));
    assert(strcmp(values[0], "3") == 0 && values[1] == NULL);
    profile_free_list(values);
    profile_release(p);
}

/* Regression test for #7971 (deleted sections should not be iterable) */
static void
test_delete_section()
{
    profile_t p;
    const char *sect[] = { "test section 1", NULL };
    const char *newrel[] = { "test section 1", "testkey", NULL };
    const char *oldrel[] = { "test section 1", "child", NULL };
    char **values;

    check(profile_init_path("test2.ini", &p));

    /* Remove and replace a section. */
    check(profile_rename_section(p, sect, NULL));
    check(profile_add_relation(p, sect, NULL));
    check(profile_add_relation(p, newrel, "6"));

    /* Check that we can read the new relation but not the old one. */
    check(profile_get_values(p, newrel, &values));
    assert(strcmp(values[0], "6") == 0 && values[1] == NULL);
    profile_free_list(values);
    check_fail(profile_get_values(p, oldrel, &values), PROF_NO_RELATION);
    profile_abandon(p);
}

/* Regression test for #7971 (profile_clear_relation() error with deleted node
 * at end of value set) */
static void
test_delete_clear_relation()
{
    profile_t p;
    const char *names[] = { "test section 1", "testkey", NULL };

    check(profile_init_path("test2.ini", &p));
    check(profile_add_relation(p, names, "1"));
    check(profile_add_relation(p, names, "2"));
    check(profile_update_relation(p, names, "2", NULL));
    check(profile_clear_relation(p, names));
    profile_abandon(p);
}

/* Test that order of relations is preserved if some relations are deleted. */
static void
test_delete_ordering()
{
    profile_t p;
    const char *names[] = { "test section 1", "testkey", NULL };
    char **values;

    check(profile_init_path("test2.ini", &p));
    check(profile_add_relation(p, names, "1"));
    check(profile_add_relation(p, names, "2"));
    check(profile_add_relation(p, names, "3"));
    check(profile_update_relation(p, names, "2", NULL));
    check(profile_add_relation(p, names, "4"));
    check(profile_get_values(p, names, &values));
    assert(strcmp(values[0], "1") == 0);
    assert(strcmp(values[1], "3") == 0);
    assert(strcmp(values[2], "4") == 0);
    assert(values[3] == NULL);
    profile_free_list(values);
    profile_abandon(p);
}

/* Regression test for #8431 (profile_flush_to_file erroneously changes flag
 * state on source object) */
static void
test_flush_to_file()
{
    profile_t p;

    /* Flush a profile object to a file without making any changes, so that the
     * source object is still within g_shared_trees. */
    check(profile_init_path("test2.ini", &p));
    unlink("test3.ini");
    check(profile_flush_to_file(p, "test3.ini"));
    profile_release(p);

    /* Check for a dangling reference in g_shared_trees by creating another
     * profile object. */
    profile_init_path("test2.ini", &p);
    profile_release(p);
}

/* Regression test for #7863 (multiply-specified subsections should
 * be merged) */
static void
test_merge_subsections()
{
    profile_t p;
    const char *n1[] = { "test section 2", "child_section2", "child", NULL };
    const char *n2[] = { "test section 2", "child_section2", "chores", NULL };
    char **values;

    check(profile_init_path("test2.ini", &p));

    check(profile_get_values(p, n1, &values));
    assert(strcmp(values[0], "slick") == 0);
    assert(strcmp(values[1], "harry") == 0);
    assert(strcmp(values[2], "john\tb ") == 0);
    assert(strcmp(values[3], "ron") == 0);
    assert(values[4] == NULL);
    profile_free_list(values);

    check(profile_get_values(p, n2, &values));
    assert(strcmp(values[0], "cleaning") == 0 && values[1] == NULL);
    profile_free_list(values);

    profile_release(p);
}

int
main()
{
    test_iterate();
    test_shared();
    test_clear();
    test_include();
    test_independence();
    test_delete_section();
    test_delete_clear_relation();
    test_delete_ordering();
    test_flush_to_file();
    test_merge_subsections();
}
