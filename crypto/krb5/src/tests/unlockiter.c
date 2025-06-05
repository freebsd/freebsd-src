/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/unlockiter.c - test program for unlocked iteration */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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
 * Test unlocked KDB iteration.
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <errno.h>
#include <krb5.h>
#include <kadm5/admin.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>             /* Some platforms need memset() for FD_ZERO */
#include <unistd.h>

struct cb_arg {
    int inpipe;
    int outpipe;
    int timeout;
    int done;
};

/* Helper function for cb(): read a sync byte (with possible timeout), then
 * write a sync byte. */
static int
syncpair_rw(const char *name, struct cb_arg *arg, char *cp, int timeout)
{
    struct timeval tv;
    fd_set rset;
    int nfds;

    FD_ZERO(&rset);
    FD_SET(arg->inpipe, &rset);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    printf("cb: waiting for %s sync pair\n", name);
    nfds = select(arg->inpipe + 1, &rset,
                  NULL, NULL, (timeout == 0) ? NULL : &tv);
    if (nfds < 0)
        return -1;
    if (nfds == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (read(arg->inpipe, cp, 1) < 0)
        return -1;
    printf("cb: writing %s sync pair\n", name);
    if (write(arg->outpipe, cp, 1) < 0)
        return -1;
    return 0;
}

/* On the first iteration only, receive and send sync bytes to the locking
 * child to drive its locking activities. */
static krb5_error_code
cb(void *argin, krb5_db_entry *ent)
{
    struct cb_arg *arg = argin;
    char c = '\0';

    if (arg->done)
        return 0;

    if (syncpair_rw("first", arg, &c, 0) < 0) {
        com_err("cb", errno, "first sync pair");
        return errno;
    }
    if (syncpair_rw("second", arg, &c, arg->timeout) < 0) {
        com_err("cb", errno, "second sync pair");
        return errno;
    }
    printf("cb: waiting for final sync byte\n");
    if (read(arg->inpipe, &c, 1) < 0) {
        com_err("cb", errno, "final sync byte");
        return errno;
    }
    arg->done = 1;
    return 0;
}

/* Parent process: iterate over the KDB, using a callback that synchronizes
 * with the locking child. */
static int
iterator(struct cb_arg *cb_arg, char **db_args, pid_t child)
{
    krb5_error_code retval;
    krb5_context ctx;

    retval = krb5_init_context_profile(NULL, KRB5_INIT_CONTEXT_KDC, &ctx);
    if (retval)
        goto cleanup;

    retval = krb5_db_open(ctx, db_args,
                          KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN);
    if (retval)
        goto cleanup;

    retval = krb5_db_iterate(ctx, NULL, cb, cb_arg, 0);
    if (retval)
        goto cleanup;

    retval = krb5_db_fini(ctx);

cleanup:
    krb5_free_context(ctx);
    if (retval) {
        com_err("iterator", retval, "");
        kill(child, SIGTERM);
        exit(1);
    }
    return retval;
}

/* Helper function for locker(): write, then receive a sync byte. */
static int
syncpair_wr(const char *name, int inpipe, int outpipe, unsigned char *cp)
{
    printf("locker: writing %s sync pair\n", name);
    if (write(outpipe, cp, 1) < 0)
        return -1;
    printf("locker: waiting for %s sync pair\n", name);
    if (read(inpipe, cp, 1) < 0)
        return -1;
    return 0;
}

/* Child process: acquire and release a write lock on the KDB, synchronized
 * with parent. */
static int
locker(int inpipe, int outpipe, char **db_args)
{
    krb5_error_code retval;
    unsigned char c = '\0';
    krb5_context ctx;

    retval = krb5_init_context_profile(NULL, KRB5_INIT_CONTEXT_KDC, &ctx);
    if (retval)
        goto cleanup;

    retval = krb5_db_open(ctx, db_args,
                          KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN);
    if (retval)
        goto cleanup;

    if (syncpair_wr("first", inpipe, outpipe, &c) < 0) {
        retval = errno;
        goto cleanup;
    }
    printf("locker: acquiring lock...\n");
    retval = krb5_db_lock(ctx, KRB5_DB_LOCKMODE_EXCLUSIVE);
    if (retval)
        goto cleanup;
    printf("locker: acquired lock\n");
    if (syncpair_wr("second", inpipe, outpipe, &c) < 0) {
        retval = errno;
        goto cleanup;
    }
    krb5_db_unlock(ctx);
    printf("locker: released lock\n");
    printf("locker: writing final sync byte\n");
    if (write(outpipe, &c, 1) < 0) {
        retval = errno;
        goto cleanup;
    }
    retval = krb5_db_fini(ctx);
cleanup:
    if (retval)
        com_err("locker", retval, "");

    krb5_free_context(ctx);
    exit(retval != 0);
}

static void
usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-lu] [-t timeout]\n", prog);
    exit(1);
}

int
main(int argc, char *argv[])
{
    struct cb_arg cb_arg;
    pid_t child;
    char *db_args[2] = { NULL, NULL };
    int c;
    int cstatus;
    int pipe_to_locker[2], pipe_to_iterator[2];

    cb_arg.timeout = 1;
    cb_arg.done = 0;
    while ((c = getopt(argc, argv, "lt:u")) != -1) {
        switch (c) {
        case 'l':
            db_args[0] = "lockiter";
            break;
        case 't':
            cb_arg.timeout = atoi(optarg);
            break;
        case 'u':
            db_args[0] = "unlockiter";
            break;
        default:
            usage(argv[0]);
        }
    }
    if (pipe(pipe_to_locker) < 0) {
        com_err(argv[0], errno, "pipe(p_il)");
        exit(1);
    }
    if (pipe(pipe_to_iterator) < 0) {
        com_err(argv[0], errno, "pipe(p_li)");
        exit(1);
    }
    cb_arg.inpipe = pipe_to_iterator[0];
    cb_arg.outpipe = pipe_to_locker[1];
    child = fork();
    switch (child) {
    case -1:
        com_err(argv[0], errno, "fork");
        exit(1);
        break;
    case 0:
        locker(pipe_to_locker[0], pipe_to_iterator[1], db_args);
        break;
    default:
        if (iterator(&cb_arg, db_args, child))
            exit(1);
        if (wait(&cstatus) < 0) {
            com_err(argv[0], errno, "wait");
            exit(1);
        }
        if (WIFSIGNALED(cstatus))
            exit(1);
        if (WIFEXITED(cstatus) && WEXITSTATUS(cstatus) != 0) {
            exit(1);
        }
    }
    exit(0);
}
