/*
 * PAM utility vector library test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014, 2016, 2018-2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011, 2013
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

#include <sys/wait.h>

#include <pam-util/vector.h>
#include <tests/tap/basic.h>
#include <tests/tap/string.h>


int
main(void)
{
    struct vector *vector, *ovector, *copy;
    char *command, *string;
    const char *env[2];
    pid_t child;
    size_t i;
    const char cstring[] = "This is a\ttest.  ";

    plan(60);

    vector = vector_new();
    ok(vector != NULL, "vector_new returns non-NULL");
    if (vector == NULL)
        bail("vector_new returned NULL");
    ok(vector_add(vector, cstring), "vector_add succeeds");
    is_int(1, vector->count, "vector_add increases count");
    ok(vector->strings[0] != cstring, "...and allocated new memory");
    ok(vector_resize(vector, 4), "vector_resize succeeds");
    is_int(4, vector->allocated, "vector_resize works");
    ok(vector_add(vector, cstring), "vector_add #2");
    ok(vector_add(vector, cstring), "vector_add #3");
    ok(vector_add(vector, cstring), "vector_add #4");
    is_int(4, vector->allocated, "...and no reallocation when adding strings");
    is_int(4, vector->count, "...and the count matches");
    is_string(cstring, vector->strings[0], "added the right string");
    is_string(cstring, vector->strings[1], "added the right string");
    is_string(cstring, vector->strings[2], "added the right string");
    is_string(cstring, vector->strings[3], "added the right string");
    ok(vector->strings[1] != vector->strings[2], "each pointer is different");
    ok(vector->strings[2] != vector->strings[3], "each pointer is different");
    ok(vector->strings[3] != vector->strings[0], "each pointer is different");
    ok(vector->strings[0] != cstring, "each pointer is different");
    copy = vector_copy(vector);
    ok(copy != NULL, "vector_copy returns non-NULL");
    if (copy == NULL)
        bail("vector_copy returned NULL");
    is_int(4, copy->count, "...and has right count");
    is_int(4, copy->allocated, "...and has right allocated count");
    for (i = 0; i < 4; i++) {
        is_string(cstring, copy->strings[i], "...and string %lu is right",
                  (unsigned long) i);
        ok(copy->strings[i] != vector->strings[i],
           "...and pointer %lu is different", (unsigned long) i);
    }
    vector_free(copy);
    vector_clear(vector);
    is_int(0, vector->count, "vector_clear works");
    is_int(4, vector->allocated, "...but doesn't free the allocation");
    string = strdup(cstring);
    if (string == NULL)
        sysbail("cannot allocate memory");
    ok(vector_add(vector, cstring), "vector_add succeeds");
    ok(vector_add(vector, string), "vector_add succeeds");
    is_int(2, vector->count, "added two strings to the vector");
    ok(vector->strings[1] != string, "...and the pointers are different");
    ok(vector_resize(vector, 1), "vector_resize succeeds");
    is_int(1, vector->count, "vector_resize shrinks the vector");
    ok(vector->strings[0] != cstring, "...and the pointer is different");
    vector_free(vector);
    free(string);

    vector = vector_split_multi("foo, bar, baz", ", ", NULL);
    ok(vector != NULL, "vector_split_multi returns non-NULL");
    if (vector == NULL)
        bail("vector_split_multi returned NULL");
    is_int(3, vector->count, "vector_split_multi returns right count");
    is_string("foo", vector->strings[0], "...first string");
    is_string("bar", vector->strings[1], "...second string");
    is_string("baz", vector->strings[2], "...third string");
    ovector = vector;
    vector = vector_split_multi("", ", ", vector);
    ok(vector != NULL, "reuse of vector doesn't return NULL");
    ok(vector == ovector, "...and reuses the same vector pointer");
    is_int(0, vector->count, "vector_split_multi reuse with empty string");
    is_int(3, vector->allocated, "...and doesn't free allocation");
    vector = vector_split_multi(",,,  foo,   ", ", ", vector);
    ok(vector != NULL, "reuse of vector doesn't return NULL");
    is_int(1, vector->count, "vector_split_multi with extra separators");
    is_string("foo", vector->strings[0], "...first string");
    vector = vector_split_multi(", ,  ", ", ", vector);
    is_int(0, vector->count, "vector_split_multi with only separators");
    vector_free(vector);

    vector = vector_new();
    ok(vector_add(vector, "/bin/sh"), "vector_add succeeds");
    ok(vector_add(vector, "-c"), "vector_add succeeds");
    basprintf(&command, "echo ok %lu - vector_exec", testnum++);
    ok(vector_add(vector, command), "vector_add succeeds");
    child = fork();
    if (child < 0)
        sysbail("unable to fork");
    else if (child == 0)
        if (vector_exec("/bin/sh", vector) < 0)
            sysdiag("unable to exec /bin/sh");
    waitpid(child, NULL, 0);
    vector_free(vector);
    free(command);

    vector = vector_new();
    ok(vector_add(vector, "/bin/sh"), "vector_add succeeds");
    ok(vector_add(vector, "-c"), "vector_add succeeds");
    ok(vector_add(vector, "echo ok $NUMBER - vector_exec_env"),
       "vector_add succeeds");
    basprintf(&string, "NUMBER=%lu", testnum++);
    env[0] = string;
    env[1] = NULL;
    child = fork();
    if (child < 0)
        sysbail("unable to fork");
    else if (child == 0)
        if (vector_exec_env("/bin/sh", vector, env) < 0)
            sysdiag("unable to exec /bin/sh");
    waitpid(child, NULL, 0);
    vector_free(vector);
    free(string);

    return 0;
}
