/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/profile/test_vtable.c - Test program for vtable-backed profiles */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * This test program exercises vtable profile functionality using two vtables,
 * one which implements just the basic methods and one which implements all of
 * the methods.  The program doesn't attempt to create a working profile
 * implementation; it just verifies the expected control flow into the vtable
 * and back out to the caller.
 */

#include <k5-platform.h>
#include "profile.h"

static int basic_cbdata;
static int full_cbdata;
static const char *empty_names[] = { NULL };
static const char *name_string = "get_string";
static const char *name_int = "get_int";
static const char *name_bool = "get_bool";

static long
basic_get_values(void *cbdata, const char *const *names, char ***ret_values)
{
    assert(cbdata == &basic_cbdata);
    assert(names == empty_names);
    *ret_values = calloc(3, sizeof(*ret_values));
    (*ret_values)[0] = strdup("one");
    (*ret_values)[1] = strdup("two");
    (*ret_values)[2] = NULL;
    return 0;
}

static void
free_values(void *cbdata, char **values)
{
    char **v;

    for (v = values; *v; v++)
        free(*v);
    free(values);
}

static long
full_get_values(void *cbdata, const char *const *names, char ***ret_values)
{
    assert(cbdata == &full_cbdata);
    *ret_values = calloc(2, sizeof(*ret_values));
    if (names[0] == name_string)
        (*ret_values)[0] = strdup("string result");
    else if (names[0] == name_int)
        (*ret_values)[0] = strdup("23");
    else if (names[0] == name_bool)
        (*ret_values)[0] = strdup("on");
    else {
        free(*ret_values);
        return PROF_NO_RELATION;
    }
    (*ret_values)[1] = NULL;
    return 0;
}

static void
full_cleanup(void *cbdata)
{
    assert(cbdata == &full_cbdata);
}

static long
full_copy(void *cbdata, void **ret_cbdata)
{
    assert(cbdata == &full_cbdata);
    *ret_cbdata = &full_cbdata;
    return 0;
}

struct iterator {
    int count;
};

static long
full_iterator_create(void *cbdata, const char *const *names, int flags,
                     void **ret_iter)
{
    struct iterator *iter;

    assert(cbdata == &full_cbdata);
    assert(names == empty_names);
    assert(flags == 126);
    iter = malloc(sizeof(*iter));
    iter->count = 0;
    *ret_iter = iter;
    return 0;
}

static long
full_iterator(void *cbdata, void *iter_arg, char **ret_name, char **ret_value)
{
    struct iterator *iter = iter_arg;

    assert(cbdata == &full_cbdata);
    assert(iter->count >= 0 && iter->count <= 2);
    if (iter->count == 0) {
        *ret_name = strdup("name1");
        *ret_value = strdup("value1");
    } else if (iter->count == 1) {
        *ret_name = strdup("name2");
        *ret_value = NULL;
    } else {
        *ret_name = NULL;
        *ret_value = NULL;
    }
    iter->count++;
    return 0;
}

static void
full_iterator_free(void *cbdata, void *iter_arg)
{
    struct iterator *iter = iter_arg;

    assert(cbdata == &full_cbdata);
    assert(iter->count == 3);
    free(iter);
}

static void
full_free_string(void *cbdata, char *string)
{
    assert(cbdata == &full_cbdata);
    free(string);
}

static long
full_writable(void *cbdata, int *writable)
{
    assert(cbdata == &full_cbdata);
    *writable = 12;
    return 0;
}

static long
full_modified(void *cbdata, int *modified)
{
    assert(cbdata == &full_cbdata);
    *modified = 6;
    return 0;
}

static long
full_update_relation(void *cbdata, const char **names,
                     const char *old_value, const char *new_value)
{
    assert(cbdata == &full_cbdata);
    assert(names == empty_names);
    assert(old_value == name_string || old_value == NULL);
    assert(new_value == NULL);
    return 0;
}

static long
full_rename_section(void *cbdata, const char **names, const char *new_name)
{
    assert(cbdata == &full_cbdata);
    assert(names == empty_names);
    assert(new_name == name_int);
    return 0;
}

static long
full_add_relation(void *cbdata, const char **names, const char *new_value)
{
    assert(cbdata == &full_cbdata);
    assert(names == empty_names);
    assert(new_value == name_bool);
    return 0;
}

static long
full_flush(void *cbdata)
{
    assert(cbdata == &full_cbdata);
    return 0;
}

struct profile_vtable basic_vtable = {
    1,
    basic_get_values,
    free_values,
};

struct profile_vtable full_vtable = {
    1,
    full_get_values,
    free_values,
    full_cleanup,
    full_copy,

    full_iterator_create,
    full_iterator,
    full_iterator_free,
    full_free_string,

    full_writable,
    full_modified,
    full_update_relation,
    full_rename_section,
    full_add_relation,
    full_flush
};

int main()
{
    profile_t profile;
    char **values, *str, *name, *value;
    void *iter;
    int intval;

    assert(profile_init_vtable(&basic_vtable, &basic_cbdata, &profile) == 0);
    assert(profile_get_values(profile, empty_names, &values) == 0);
    assert(strcmp(values[0], "one") == 0);
    assert(strcmp(values[1], "two") == 0);
    assert(values[2] == NULL);
    profile_free_list(values);
    assert(profile_iterator_create(profile, NULL, 0, &iter) ==
           PROF_UNSUPPORTED);
    assert(profile_is_writable(profile, &intval) == 0);
    assert(intval == 0);
    assert(profile_is_modified(profile, &intval) == 0);
    assert(intval == 0);
    assert(profile_update_relation(profile, NULL, NULL, NULL) ==
           PROF_UNSUPPORTED);
    assert(profile_clear_relation(profile, NULL) == PROF_UNSUPPORTED);
    assert(profile_rename_section(profile, NULL, NULL) == PROF_UNSUPPORTED);
    assert(profile_add_relation(profile, NULL, NULL) == PROF_UNSUPPORTED);
    profile_flush(profile);
    profile_abandon(profile);

    assert(profile_init_vtable(&full_vtable, &full_cbdata, &profile) == 0);
    assert(profile_get_string(profile, name_string, NULL, NULL, "wrong",
                              &str) == 0);
    assert(strcmp(str, "string result") == 0);
    profile_release_string(str);
    assert(profile_get_integer(profile, name_int, NULL, NULL, 24,
                               &intval) == 0);
    assert(intval == 23);
    assert(profile_get_boolean(profile, name_bool, NULL, NULL, 0,
                               &intval) == 0);
    assert(intval == 1);
    assert(profile_get_integer(profile, "xxx", NULL, NULL, 62, &intval) == 0);
    assert(intval == 62);

    assert(profile_iterator_create(profile, empty_names, 126, &iter) == 0);
    assert(profile_iterator(&iter, &name, &value) == 0);
    assert(strcmp(name, "name1") == 0);
    assert(strcmp(value, "value1") == 0);
    profile_release_string(name);
    profile_release_string(value);
    assert(profile_iterator(&iter, &name, &value) == 0);
    assert(strcmp(name, "name2") == 0);
    assert(value == NULL);
    profile_release_string(name);
    assert(profile_iterator(&iter, &name, &value) == 0);
    assert(iter == NULL);
    assert(name == NULL);
    assert(value == NULL);

    assert(profile_is_writable(profile, &intval) == 0);
    assert(intval == 12);
    assert(profile_is_modified(profile, &intval) == 0);
    assert(intval == 6);
    assert(profile_update_relation(profile, empty_names, name_string,
                                   NULL) == 0);
    assert(profile_clear_relation(profile, empty_names) == 0);
    assert(profile_rename_section(profile, empty_names, name_int) == 0);
    assert(profile_add_relation(profile, empty_names, name_bool) == 0);
    profile_release(profile);

    return 0;
}
