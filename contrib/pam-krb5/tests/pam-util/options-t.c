/*
 * PAM option parsing test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2014
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

#include <syslog.h>

#include <pam-util/args.h>
#include <pam-util/options.h>
#include <pam-util/vector.h>
#include <tests/fakepam/pam.h>
#include <tests/tap/basic.h>
#include <tests/tap/string.h>

/* The configuration struct we will use for testing. */
struct pam_config {
    struct vector *cells;
    bool debug;
#ifdef HAVE_KRB5
    krb5_deltat expires;
#else
    long expires;
#endif
    bool ignore_root;
    long minimum_uid;
    char *program;
};

#define K(name) (#name), offsetof(struct pam_config, name)

/* The rules specifying the configuration options. */
static struct option options[] = {
    /* clang-format off */
    { K(cells),       true,  LIST   (NULL)  },
    { K(debug),       true,  BOOL   (false) },
    { K(expires),     true,  TIME   (10)    },
    { K(ignore_root), false, BOOL   (true)  },
    { K(minimum_uid), true,  NUMBER (0)     },
    { K(program),     true,  STRING (NULL)  },
    /* clang-format on */
};
static const size_t optlen = sizeof(options) / sizeof(options[0]);

/*
 * A macro used to parse the various ways of spelling booleans.  This reuses
 * the argv_bool variable, setting it to the first value provided and then
 * calling putil_args_parse() on it.  It then checks whether the provided
 * config option is set to the expected value.
 */
#define TEST_BOOL(a, c, v)                                              \
    do {                                                                \
        argv_bool[0] = (a);                                             \
        status = putil_args_parse(args, 1, argv_bool, options, optlen); \
        ok(status, "Parse of %s", (a));                                 \
        is_int((v), (c), "...and value is correct");                    \
        ok(pam_output() == NULL, "...and no output");                   \
    } while (0)

/*
 * A macro used to test error reporting from putil_args_parse().  This reuses
 * the argv_err variable, setting it to the first value provided and then
 * calling putil_args_parse() on it.  It then recovers the error message and
 * expects it to match the severity and error message given.
 */
#define TEST_ERROR(a, p, e)                                                  \
    do {                                                                     \
        argv_err[0] = (a);                                                   \
        status = putil_args_parse(args, 1, argv_err, options, optlen);       \
        ok(status, "Parse of %s", (a));                                      \
        seen = pam_output();                                                 \
        if (seen == NULL)                                                    \
            ok_block(2, false, "...no error output");                        \
        else {                                                               \
            is_int((p), seen->lines[0].priority, "...priority for %s", (a)); \
            is_string((e), seen->lines[0].line, "...error for %s", (a));     \
        }                                                                    \
        pam_output_free(seen);                                               \
    } while (0)


/*
 * Allocate and initialize a new struct config.
 */
static struct pam_config *
config_new(void)
{
    return bcalloc(1, sizeof(struct pam_config));
}


/*
 * Free a struct config and all of its members.
 */
static void
config_free(struct pam_config *config)
{
    if (config == NULL)
        return;
    vector_free(config->cells);
    free(config->program);
    free(config);
}


int
main(void)
{
    pam_handle_t *pamh;
    struct pam_args *args;
    struct pam_conv conv = {NULL, NULL};
    bool status;
    struct vector *cells;
    char *program;
    struct output *seen;
    const char *argv_bool[2] = {NULL, NULL};
    const char *argv_err[2] = {NULL, NULL};
    const char *argv_empty[] = {NULL};
#ifdef HAVE_KRB5
    const char *argv_all[] = {"cells=stanford.edu,ir.stanford.edu",
                              "debug",
                              "expires=1d",
                              "ignore_root",
                              "minimum_uid=1000",
                              "program=/bin/true"};
    char *krb5conf;
#else
    const char *argv_all[] = {"cells=stanford.edu,ir.stanford.edu",
                              "debug",
                              "expires=86400",
                              "ignore_root",
                              "minimum_uid=1000",
                              "program=/bin/true"};
#endif

    if (pam_start("test", NULL, &conv, &pamh) != PAM_SUCCESS)
        sysbail("cannot create pam_handle_t");
    args = putil_args_new(pamh, 0);
    if (args == NULL)
        bail("cannot create PAM argument struct");

    plan(161);

    /* First, check just the defaults. */
    args->config = config_new();
    status = putil_args_defaults(args, options, optlen);
    ok(status, "Setting the defaults");
    ok(args->config->cells == NULL, "...cells default");
    is_int(false, args->config->debug, "...debug default");
    is_int(10, args->config->expires, "...expires default");
    is_int(true, args->config->ignore_root, "...ignore_root default");
    is_int(0, args->config->minimum_uid, "...minimum_uid default");
    ok(args->config->program == NULL, "...program default");

    /* Now parse an empty set of PAM arguments.  Nothing should change. */
    status = putil_args_parse(args, 0, argv_empty, options, optlen);
    ok(status, "Parse of empty argv");
    ok(args->config->cells == NULL, "...cells still default");
    is_int(false, args->config->debug, "...debug still default");
    is_int(10, args->config->expires, "...expires default");
    is_int(true, args->config->ignore_root, "...ignore_root still default");
    is_int(0, args->config->minimum_uid, "...minimum_uid still default");
    ok(args->config->program == NULL, "...program still default");

    /* Now, check setting everything. */
    status = putil_args_parse(args, 6, argv_all, options, optlen);
    ok(status, "Parse of full argv");
    if (args->config->cells == NULL)
        ok_block(4, false, "...cells is set");
    else {
        ok(args->config->cells != NULL, "...cells is set");
        is_int(2, args->config->cells->count, "...with two cells");
        is_string("stanford.edu", args->config->cells->strings[0],
                  "...first is stanford.edu");
        is_string("ir.stanford.edu", args->config->cells->strings[1],
                  "...second is ir.stanford.edu");
    }
    is_int(true, args->config->debug, "...debug is set");
    is_int(86400, args->config->expires, "...expires is set");
    is_int(true, args->config->ignore_root, "...ignore_root is set");
    is_int(1000, args->config->minimum_uid, "...minimum_uid is set");
    is_string("/bin/true", args->config->program, "...program is set");
    config_free(args->config);
    args->config = NULL;

    /* Test deep copying of defaults. */
    cells = vector_new();
    if (cells == NULL)
        sysbail("cannot allocate memory");
    vector_add(cells, "foo.com");
    vector_add(cells, "bar.com");
    options[0].defaults.list = cells;
    program = strdup("/bin/false");
    if (program == NULL)
        sysbail("cannot allocate memory");
    options[5].defaults.string = program;
    args->config = config_new();
    status = putil_args_defaults(args, options, optlen);
    ok(status, "Setting defaults with new defaults");
    if (args->config->cells == NULL)
        ok_block(4, false, "...cells is set");
    else {
        ok(args->config->cells != NULL, "...cells is set");
        is_int(2, args->config->cells->count, "...with two cells");
        is_string("foo.com", args->config->cells->strings[0],
                  "...first is foo.com");
        is_string("bar.com", args->config->cells->strings[1],
                  "...second is bar.com");
    }
    is_string("/bin/false", args->config->program, "...program is /bin/false");
    status = putil_args_parse(args, 6, argv_all, options, optlen);
    ok(status, "Parse of full argv after defaults");
    if (args->config->cells == NULL)
        ok_block(4, false, "...cells is set");
    else {
        ok(args->config->cells != NULL, "...cells is set");
        is_int(2, args->config->cells->count, "...with two cells");
        is_string("stanford.edu", args->config->cells->strings[0],
                  "...first is stanford.edu");
        is_string("ir.stanford.edu", args->config->cells->strings[1],
                  "...second is ir.stanford.edu");
    }
    is_int(true, args->config->debug, "...debug is set");
    is_int(86400, args->config->expires, "...expires is set");
    is_int(true, args->config->ignore_root, "...ignore_root is set");
    is_int(1000, args->config->minimum_uid, "...minimum_uid is set");
    is_string("/bin/true", args->config->program, "...program is set");
    is_string("foo.com", cells->strings[0], "...first cell after parse");
    is_string("bar.com", cells->strings[1], "...second cell after parse");
    is_string("/bin/false", program, "...string after parse");
    config_free(args->config);
    args->config = NULL;
    is_string("foo.com", cells->strings[0], "...first cell after free");
    is_string("bar.com", cells->strings[1], "...second cell after free");
    is_string("/bin/false", program, "...string after free");
    options[0].defaults.list = NULL;
    options[5].defaults.string = NULL;
    vector_free(cells);
    free(program);

    /* Test specifying the default for a vector parameter as a string. */
    options[0].type = TYPE_STRLIST;
    options[0].defaults.string = "foo.com,bar.com";
    args->config = config_new();
    status = putil_args_defaults(args, options, optlen);
    ok(status, "Setting defaults with string default for vector");
    if (args->config->cells == NULL)
        ok_block(4, false, "...cells is set");
    else {
        ok(args->config->cells != NULL, "...cells is set");
        is_int(2, args->config->cells->count, "...with two cells");
        is_string("foo.com", args->config->cells->strings[0],
                  "...first is foo.com");
        is_string("bar.com", args->config->cells->strings[1],
                  "...second is bar.com");
    }
    config_free(args->config);
    args->config = NULL;
    options[0].type = TYPE_LIST;
    options[0].defaults.string = NULL;

    /* Should be no errors so far. */
    ok(pam_output() == NULL, "No errors so far");

    /* Test various ways of spelling booleans. */
    args->config = config_new();
    TEST_BOOL("debug", args->config->debug, true);
    TEST_BOOL("debug=false", args->config->debug, false);
    TEST_BOOL("debug=true", args->config->debug, true);
    TEST_BOOL("debug=no", args->config->debug, false);
    TEST_BOOL("debug=yes", args->config->debug, true);
    TEST_BOOL("debug=off", args->config->debug, false);
    TEST_BOOL("debug=on", args->config->debug, true);
    TEST_BOOL("debug=0", args->config->debug, false);
    TEST_BOOL("debug=1", args->config->debug, true);
    TEST_BOOL("debug=False", args->config->debug, false);
    TEST_BOOL("debug=trUe", args->config->debug, true);
    TEST_BOOL("debug=No", args->config->debug, false);
    TEST_BOOL("debug=Yes", args->config->debug, true);
    TEST_BOOL("debug=OFF", args->config->debug, false);
    TEST_BOOL("debug=ON", args->config->debug, true);
    config_free(args->config);
    args->config = NULL;

    /* Test for various parsing errors. */
    args->config = config_new();
    TEST_ERROR("debug=", LOG_ERR, "invalid boolean in setting: debug=");
    TEST_ERROR("debug=truth", LOG_ERR,
               "invalid boolean in setting: debug=truth");
    TEST_ERROR("minimum_uid", LOG_ERR, "value missing for option minimum_uid");
    TEST_ERROR("minimum_uid=", LOG_ERR,
               "value missing for option minimum_uid=");
    TEST_ERROR("minimum_uid=foo", LOG_ERR,
               "invalid number in setting: minimum_uid=foo");
    TEST_ERROR("minimum_uid=1000foo", LOG_ERR,
               "invalid number in setting: minimum_uid=1000foo");
    TEST_ERROR("program", LOG_ERR, "value missing for option program");
    TEST_ERROR("cells", LOG_ERR, "value missing for option cells");
    config_free(args->config);
    args->config = NULL;

#ifdef HAVE_KRB5

    /* Test for Kerberos krb5.conf option parsing. */
    krb5conf = test_file_path("data/krb5-pam.conf");
    if (krb5conf == NULL)
        bail("cannot find data/krb5-pam.conf");
    if (setenv("KRB5_CONFIG", krb5conf, 1) < 0)
        sysbail("cannot set KRB5_CONFIG");
    krb5_free_context(args->ctx);
    status = krb5_init_context(&args->ctx);
    if (status != 0)
        bail("cannot parse test krb5.conf file");
    args->config = config_new();
    status = putil_args_defaults(args, options, optlen);
    ok(status, "Setting the defaults");
    status = putil_args_krb5(args, "testing", options, optlen);
    ok(status, "Options from krb5.conf");
    ok(args->config->cells == NULL, "...cells default");
    is_int(true, args->config->debug, "...debug set from krb5.conf");
    is_int(1800, args->config->expires, "...expires set from krb5.conf");
    is_int(true, args->config->ignore_root, "...ignore_root default");
    is_int(1000, args->config->minimum_uid,
           "...minimum_uid set from krb5.conf");
    ok(args->config->program == NULL, "...program default");
    status = putil_args_krb5(args, "other-test", options, optlen);
    ok(status, "Options from krb5.conf (other-test)");
    is_int(-1000, args->config->minimum_uid,
           "...minimum_uid set from krb5.conf other-test");

    /* Test with a realm set, which should expose more settings. */
    krb5_free_context(args->ctx);
    status = krb5_init_context(&args->ctx);
    if (status != 0)
        bail("cannot parse test krb5.conf file");
    args->realm = strdup("FOO.COM");
    if (args->realm == NULL)
        sysbail("cannot allocate memory");
    status = putil_args_krb5(args, "testing", options, optlen);
    ok(status, "Options from krb5.conf with FOO.COM");
    is_int(2, args->config->cells->count, "...cells count from krb5.conf");
    is_string("foo.com", args->config->cells->strings[0],
              "...first cell from krb5.conf");
    is_string("bar.com", args->config->cells->strings[1],
              "...second cell from krb5.conf");
    is_int(true, args->config->debug, "...debug set from krb5.conf");
    is_int(1800, args->config->expires, "...expires set from krb5.conf");
    is_int(true, args->config->ignore_root, "...ignore_root default");
    is_int(1000, args->config->minimum_uid,
           "...minimum_uid set from krb5.conf");
    is_string("/bin/false", args->config->program,
              "...program from krb5.conf");

    /* Test with a different realm. */
    free(args->realm);
    args->realm = strdup("BAR.COM");
    if (args->realm == NULL)
        sysbail("cannot allocate memory");
    status = putil_args_krb5(args, "testing", options, optlen);
    ok(status, "Options from krb5.conf with BAR.COM");
    is_int(2, args->config->cells->count, "...cells count from krb5.conf");
    is_string("bar.com", args->config->cells->strings[0],
              "...first cell from krb5.conf");
    is_string("foo.com", args->config->cells->strings[1],
              "...second cell from krb5.conf");
    is_int(true, args->config->debug, "...debug set from krb5.conf");
    is_int(1800, args->config->expires, "...expires set from krb5.conf");
    is_int(true, args->config->ignore_root, "...ignore_root default");
    is_int(1000, args->config->minimum_uid,
           "...minimum_uid set from krb5.conf");
    is_string("echo /bin/true", args->config->program,
              "...program from krb5.conf");
    config_free(args->config);
    args->config = config_new();
    status = putil_args_krb5(args, "other-test", options, optlen);
    ok(status, "Options from krb5.conf (other-test with realm)");
    ok(args->config->cells == NULL, "...cells is NULL");
    is_string("echo /bin/true", args->config->program,
              "...program from krb5.conf");
    config_free(args->config);
    args->config = NULL;

    /* Test for time parsing errors. */
    args->config = config_new();
    TEST_ERROR("expires=ft87", LOG_ERR,
               "bad time value in setting: expires=ft87");
    config_free(args->config);

    /* Test error reporting from the krb5.conf parser. */
    args->config = config_new();
    status = putil_args_krb5(args, "bad-number", options, optlen);
    ok(status, "Options from krb5.conf (bad-number)");
    seen = pam_output();
    is_string("invalid number in krb5.conf setting for minimum_uid: 1000foo",
              seen->lines[0].line, "...and correct error reported");
    is_int(LOG_ERR, seen->lines[0].priority, "...with correct priority");
    pam_output_free(seen);
    config_free(args->config);
    args->config = NULL;

    /* Test error reporting on times from the krb5.conf parser. */
    args->config = config_new();
    status = putil_args_krb5(args, "bad-time", options, optlen);
    ok(status, "Options from krb5.conf (bad-time)");
    seen = pam_output();
    if (seen == NULL)
        ok_block(2, false, "...no error output");
    else {
        is_string("invalid time in krb5.conf setting for expires: ft87",
                  seen->lines[0].line, "...and correct error reported");
        is_int(LOG_ERR, seen->lines[0].priority, "...with correct priority");
    }
    pam_output_free(seen);
    config_free(args->config);
    args->config = NULL;

    test_file_path_free(krb5conf);

#else /* !HAVE_KRB5 */

    skip_block(37, "Kerberos support not configured");

#endif

    putil_args_free(args);
    pam_end(pamh, 0);
    return 0;
}
