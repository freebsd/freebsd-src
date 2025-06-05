/*
 * Tests for trace logging in the pam-krb5 module.
 *
 * Checks that trace logging is handled properly.  This is currently very
 * simple and just checks that the file is created.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/basic.h>
#include <tests/tap/string.h>


int
main(void)
{
    struct script_config config;
    char *tmpdir, *trace;

    plan_lazy();

    memset(&config, 0, sizeof(config));
    config.user = "testuser";
    tmpdir = test_tmpdir();
    basprintf(&trace, "%s/trace", tmpdir);
    config.extra[0] = trace;
#ifdef HAVE_KRB5_SET_TRACE_FILENAME
    run_script("data/scripts/trace/supported", &config);
    is_int(0, access(trace, F_OK), "Trace file was created");
    unlink(trace);
#else
    run_script("data/scripts/trace/unsupported", &config);
    is_int(-1, access(trace, F_OK), "Trace file does not exist");
#endif

    free(trace);
    test_tmpdir_free(tmpdir);
    return 0;
}
