/*
 * Fake PAM library test suite.
 *
 * This is not actually a test for the pam-util layer, but rather is a test
 * for the trickier components of the fake PAM library that in turn is used to
 * test the pam-util layer and PAM modules.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010, 2013
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

#include <config.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <tests/fakepam/pam.h>
#include <tests/tap/basic.h>


int
main(void)
{
    pam_handle_t *pamh;
    struct pam_conv conv = {NULL, NULL};
    char **env;
    size_t i;

    /*
     * Skip this test if the native PAM library doesn't support a PAM
     * environment, since we "break" pam_putenv to mirror the native behavior
     * in that case.
     */
#ifndef HAVE_PAM_GETENV
    skip_all("system doesn't support PAM environment");
#endif

    plan(33);

    /* Basic environment manipulation. */
    if (pam_start("test", NULL, &conv, &pamh) != PAM_SUCCESS)
        sysbail("Fake PAM initialization failed");
    is_int(PAM_BAD_ITEM, pam_putenv(pamh, "TEST"), "delete when NULL");
    ok(pam_getenv(pamh, "TEST") == NULL, "getenv when NULL");
    env = pam_getenvlist(pamh);
    ok(env != NULL, "getenvlist when NULL returns non-NULL");
    if (env == NULL)
        bail("pam_getenvlist returned NULL");
    is_string(NULL, env[0], "...but first element is NULL");
    for (i = 0; env[i] != NULL; i++)
        free(env[i]);
    free(env);

    /* putenv and getenv. */
    is_int(PAM_SUCCESS, pam_putenv(pamh, "TEST=foo"), "putenv TEST");
    is_string("foo", pam_getenv(pamh, "TEST"), "getenv TEST");
    is_int(PAM_SUCCESS, pam_putenv(pamh, "FOO=bar"), "putenv FOO");
    is_int(PAM_SUCCESS, pam_putenv(pamh, "BAR=baz"), "putenv BAR");
    is_string("foo", pam_getenv(pamh, "TEST"), "getenv TEST");
    is_string("bar", pam_getenv(pamh, "FOO"), "getenv FOO");
    is_string("baz", pam_getenv(pamh, "BAR"), "getenv BAR");
    ok(pam_getenv(pamh, "BAZ") == NULL, "getenv BAZ is NULL");

    /* Replacing and deleting environment variables. */
    is_int(PAM_BAD_ITEM, pam_putenv(pamh, "BAZ"), "putenv nonexistent delete");
    is_int(PAM_SUCCESS, pam_putenv(pamh, "FOO=foo"), "putenv replace");
    is_int(PAM_SUCCESS, pam_putenv(pamh, "FOON=bar=n"), "putenv prefix");
    is_string("foo", pam_getenv(pamh, "FOO"), "getenv FOO");
    is_string("bar=n", pam_getenv(pamh, "FOON"), "getenv FOON");
    is_int(PAM_BAD_ITEM, pam_putenv(pamh, "FO"), "putenv delete FO");
    is_int(PAM_SUCCESS, pam_putenv(pamh, "FOO"), "putenv delete FOO");
    ok(pam_getenv(pamh, "FOO") == NULL, "getenv FOO is NULL");
    is_string("bar=n", pam_getenv(pamh, "FOON"), "getenv FOON");
    is_string("baz", pam_getenv(pamh, "BAR"), "getenv BAR");

    /* pam_getenvlist. */
    env = pam_getenvlist(pamh);
    ok(env != NULL, "getenvlist not NULL");
    if (env == NULL)
        bail("pam_getenvlist returned NULL");
    is_string("TEST=foo", env[0], "getenvlist TEST");
    is_string("BAR=baz", env[1], "getenvlist BAR");
    is_string("FOON=bar=n", env[2], "getenvlist FOON");
    ok(env[3] == NULL, "getenvlist length");
    for (i = 0; env[i] != NULL; i++)
        free(env[i]);
    free(env);
    is_int(PAM_SUCCESS, pam_putenv(pamh, "FOO=foo"), "putenv FOO");
    is_string("TEST=foo", pamh->environ[0], "pamh environ TEST");
    is_string("BAR=baz", pamh->environ[1], "pamh environ BAR");
    is_string("FOON=bar=n", pamh->environ[2], "pamh environ FOON");
    is_string("FOO=foo", pamh->environ[3], "pamh environ FOO");
    ok(pamh->environ[4] == NULL, "pamh environ length");

    pam_end(pamh, 0);

    return 0;
}
