/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/profile/proftest/test.c - Test dynamic profile module */
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
 * This file implements a very simple profile module which just returns the
 * residual string and the number of copies in response to any query.  The full
 * range of vtable profile operations is tested elsewhere.
 */

#include "k5-platform.h"
#include "profile.h"

struct data {
    char *residual;
    int gen;
};

static long
get_values(void *cbdata, const char *const *names, char ***ret_values)
{
    struct data *d = cbdata;

    *ret_values = calloc(3, sizeof(*ret_values));
    (*ret_values)[0] = strdup(d->residual);
    asprintf(&(*ret_values)[1], "%d", d->gen);
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

static void
cleanup(void *cbdata)
{
    struct data *d = cbdata;

    free(d->residual);
    free(d);
}

static long
copy(void *cbdata, void **ret_cbdata)
{
    struct data *old_data = cbdata, *new_data;

    new_data = malloc(sizeof(*new_data));
    new_data->residual = strdup(old_data->residual);
    new_data->gen = old_data->gen + 1;
    *ret_cbdata = new_data;
    return 0;
}

long
profile_module_init(const char *residual, struct profile_vtable *vtable,
                    void **cb_ret);

long
profile_module_init(const char *residual, struct profile_vtable *vtable,
                    void **cb_ret)
{
    struct data *d;

    d = malloc(sizeof(*d));
    d->residual = strdup(residual);
    d->gen = 0;
    *cb_ret = d;

    vtable->get_values = get_values;
    vtable->free_values = free_values;
    vtable->cleanup = cleanup;
    vtable->copy = copy;
    return 0;
}
