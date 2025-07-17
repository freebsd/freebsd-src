/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/conccache.c - ccache concurrent get_creds/refresh test program */
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

/*
 * Usage: conccache ccname clientprinc serverprinc
 *
 * This program spawns two subprocesses.  One repeatedly runs
 * krb5_get_credentials() on ccname, and the other repeatedly refreshes ccname
 * from the default keytab.  If either subprocess fails, the program exits with
 * status 1.  The goal is to expose time windows where cache refreshes cause
 * get_cred operations to fail.
 */

#include "k5-platform.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <krb5.h>

/* Run this many iterations of each operation. */
static const int iterations = 200;

/* Saved command-line arguments. */
static const char *ccname, *server_name, *client_name;

static void
check(krb5_error_code code)
{
    if (code)
        abort();
}

static krb5_boolean
get_cred(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache cc;
    krb5_principal client, server;
    krb5_creds mcred, *cred;

    check(krb5_cc_resolve(context, ccname, &cc));
    check(krb5_parse_name(context, client_name, &client));
    check(krb5_parse_name(context, server_name, &server));

    memset(&mcred, 0, sizeof(mcred));
    mcred.client = client;
    mcred.server = server;
    ret = krb5_get_credentials(context, 0, cc, &mcred, &cred);

    krb5_free_creds(context, cred);
    krb5_free_principal(context, client);
    krb5_free_principal(context, server);
    krb5_cc_close(context, cc);

    return ret == 0;
}

static krb5_boolean
refresh_cache(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache cc;
    krb5_principal client;
    krb5_get_init_creds_opt *opt;
    krb5_creds cred;

    check(krb5_cc_resolve(context, ccname, &cc));
    check(krb5_parse_name(context, client_name, &client));

    check(krb5_get_init_creds_opt_alloc(context, &opt));
    check(krb5_get_init_creds_opt_set_out_ccache(context, opt, cc));
    ret = krb5_get_init_creds_keytab(context, &cred, client, NULL, 0, NULL,
                                     opt);

    krb5_get_init_creds_opt_free(context, opt);
    krb5_free_cred_contents(context, &cred);
    krb5_free_principal(context, client);
    krb5_cc_close(context, cc);

    return ret == 0;
}

static pid_t
spawn_cred_subprocess()
{
    krb5_context context;
    pid_t pid;
    int i;

    pid = fork();
    assert(pid >= 0);
    if (pid > 0)
        return pid;

    check(krb5_init_context(&context));
    for (i = 0; i < iterations; i++) {
        if (!get_cred(context)) {
            fprintf(stderr, "cred worker failed after %d successes\n", i);
            exit(1);
        }
    }
    krb5_free_context(context);
    exit(0);
}

static pid_t
spawn_refresh_subprocess()
{
    krb5_context context;
    pid_t pid;
    int i;

    pid = fork();
    assert(pid >= 0);
    if (pid > 0)
        return pid;

    check(krb5_init_context(&context));
    for (i = 0; i < iterations; i++) {
        if (!refresh_cache(context)) {
            fprintf(stderr, "refresh worker failed after %d successes\n", i);
            exit(1);
        }
    }
    krb5_free_context(context);
    exit(0);
}

int
main(int argc, char *argv[])
{
    krb5_context context;
    pid_t cred_pid, refresh_pid, pid;
    int cstatus, rstatus;

    assert(argc == 4);
    ccname = argv[1];
    client_name = argv[2];
    server_name = argv[3];

    /* Begin with an initialized cache. */
    check(krb5_init_context(&context));
    refresh_cache(context);
    krb5_free_context(context);

    cred_pid = spawn_cred_subprocess();
    refresh_pid = spawn_refresh_subprocess();

    pid = waitpid(cred_pid, &cstatus, 0);
    if (pid == -1)
        abort();
    pid = waitpid(refresh_pid, &rstatus, 0);
    if (pid == -1)
        abort();

    if (!WIFEXITED(cstatus) || WEXITSTATUS(cstatus) != 0)
        return 1;
    if (!WIFEXITED(rstatus) || WEXITSTATUS(rstatus) != 0)
        return 1;
    return 0;
}
