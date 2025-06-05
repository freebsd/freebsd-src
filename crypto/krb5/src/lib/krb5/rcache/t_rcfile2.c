/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/t_rcfile2.c - rcache file version 2 tests */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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
 * Usage:
 *
 *   t_rcfile2 <filename> expiry <nreps>
 *     store <nreps> records spaced far enough apart that all records appear
 *     expired; verify that the file size doesn't increase beyond one table.
 *
 *   t_rcfile2 <filename> concurrent <nprocesses> <nreps>
 *     spawn <nprocesses> subprocesses, each of which stores <nreps> unique
 *     tags.  As each process completes, the master process tests that the
 *     records stored by the subprocess appears as replays.
 *
 *   t_rcfile2 <filename> race <nprocesses> <nreps>
 *     spawn <nprocesses> subprocesses, each of which tries to store the same
 *     tag and reports success or failure.  The master process verifies that
 *     exactly one subprocess succeeds.  Repeat <reps> times.
 */

#include "rc_file2.c"
#include <sys/wait.h>
#include <sys/time.h>

krb5_context ctx;

static krb5_error_code
test_store(const char *filename, uint8_t *tag, krb5_timestamp timestamp,
           const uint32_t clockskew)
{
    krb5_data tag_data = make_data(tag, TAG_LEN);

    ctx->clockskew = clockskew;
    (void)krb5_set_debugging_time(ctx, timestamp, 0);
    return file2_store(ctx, (void *)filename, &tag_data);
}

/* Store a sequence of unique tags, with timestamps far enough apart that all
 * previous records appear expired.  Verify that we only use one table. */
static void
expiry_test(const char *filename, int reps)
{
    krb5_error_code ret;
    struct stat statbuf;
    uint8_t tag[TAG_LEN] = { 0 }, seed[K5_HASH_SEED_LEN] = { 0 }, data[4];
    uint32_t timestamp;
    const uint32_t clockskew = 5, start = 1000;
    uint64_t hashval;
    int i, st;

    assert((uint32_t)reps < (UINT32_MAX - start) / clockskew / 2);
    for (i = 0, timestamp = start; i < reps; i++, timestamp += clockskew * 2) {
        store_32_be(i, data);
        hashval = k5_siphash24(data, 4, seed);
        store_64_be(hashval, tag);

        ret = test_store(filename, tag, timestamp, clockskew);
        assert(ret == 0);

        /* Since we increment timestamp enough to expire every record between
         * each call, we should never create a second hash table. */
        st = stat(filename, &statbuf);
        assert(st == 0);
        assert(statbuf.st_size <= (FIRST_TABLE_RECORDS + 1) * RECORD_LEN);
    }
}

/* Store a sequence of unique tags with the same timestamp.  Exit with failure
 * if any store operation doesn't succeed or fail as given by expect_fail. */
static void
store_records(const char *filename, int id, int reps, int expect_fail)
{
    krb5_error_code ret;
    uint8_t tag[TAG_LEN] = { 0 };
    int i;

    store_32_be(id, tag);
    for (i = 0; i < reps; i++) {
        store_32_be(i, tag + 4);
        ret = test_store(filename, tag, 1000, 100);
        if (ret != (expect_fail ? KRB5KRB_AP_ERR_REPEAT : 0)) {
            fprintf(stderr, "store %d %d %sfail\n", id, i,
                    expect_fail ? "didn't " : "");
            _exit(1);
        }
    }
}

/* Spawn multiple child processes, each storing a sequence of unique tags.
 * After each process completes, verify that its tags appear as replays. */
static void
concurrency_test(const char *filename, int nchildren, int reps)
{
    pid_t *pids, pid;
    int i, nprocs, status;

    pids = calloc(nchildren, sizeof(*pids));
    assert(pids != NULL);
    for (i = 0; i < nchildren; i++) {
        pids[i] = fork();
        assert(pids[i] != -1);
        if (pids[i] == 0) {
            store_records(filename, i, reps, 0);
            _exit(0);
        }
    }
    for (nprocs = nchildren; nprocs > 0; nprocs--) {
        pid = wait(&status);
        assert(pid != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        for (i = 0; i < nchildren; i++) {
            if (pids[i] == pid)
                store_records(filename, i, reps, 1);
        }
    }
    free(pids);
}

/* Spawn multiple child processes, all trying to store the same tag.  Verify
 * that only one of the processes succeeded.  Repeat reps times. */
static void
race_test(const char *filename, int nchildren, int reps)
{
    int i, j, status, nsuccess;
    uint8_t tag[TAG_LEN] = { 0 };
    pid_t pid;

    for (i = 0; i < reps; i++) {
        store_32_be(i, tag);
        for (j = 0; j < nchildren; j++) {
            pid = fork();
            assert(pid != -1);
            if (pid == 0)
                _exit(test_store(filename, tag, 1000, 100) != 0);
        }

        nsuccess = 0;
        for (j = 0; j < nchildren; j++) {
            pid = wait(&status);
            assert(pid != -1);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                nsuccess++;
        }
        assert(nsuccess == 1);
    }
}

int
main(int argc, char **argv)
{
    const char *filename, *cmd;

    argv++;
    assert(*argv != NULL);

    if (krb5_init_context(&ctx) != 0)
        abort();

    assert(*argv != NULL);
    filename = *argv++;
    unlink(filename);

    assert(*argv != NULL);
    cmd = *argv++;
    if (strcmp(cmd, "expiry") == 0) {
        assert(argv[0] != NULL);
        expiry_test(filename, atoi(argv[0]));
    } else if (strcmp(cmd, "concurrent") == 0) {
        assert(argv[0] != NULL && argv[1] != NULL);
        concurrency_test(filename, atoi(argv[0]), atoi(argv[1]));
    } else if (strcmp(cmd, "race") == 0) {
        assert(argv[0] != NULL && argv[1] != NULL);
        race_test(filename, atoi(argv[0]), atoi(argv[1]));
    } else {
        abort();
    }

    krb5_free_context(ctx);
    return 0;
}
