/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/profile/test_load.c - Test harness for loadable profile modules */
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

#include "k5-platform.h"
#include "profile.h"
#include "prof_int.h"

int
main()
{
    profile_t pr, pr2;
    const char *files[] = { "./modtest.conf", NULL };
    char **values;

    assert(profile_init_flags(files, PROFILE_INIT_ALLOW_MODULE, &pr) == 0);
    assert(profile_copy(pr, &pr2) == 0);
    assert(profile_get_values(pr, NULL, &values) == 0);
    assert(strcmp(values[0], "teststring") == 0);
    assert(strcmp(values[1], "0") == 0);
    profile_free_list(values);
    assert(profile_get_values(pr2, NULL, &values) == 0);
    assert(strcmp(values[0], "teststring") == 0);
    assert(strcmp(values[1], "1") == 0);
    profile_release(pr);
    profile_abandon(pr2);
    profile_free_list(values);
    return 0;
}
