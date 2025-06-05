/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/threads/prof1.c */
/*
 * Copyright (C) 2004 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <utime.h>
#include <com_err.h>
#include <profile.h>

int nthreads = 10;
unsigned int delay = 3600;

volatile int done = 0; /* XXX hack */

const char *path = "/tmp/foo1.conf:/tmp/foo.conf";
const char *filename = "/tmp/foo.conf";

const char *prog;

static void *worker(void *arg)
{
    profile_t p;
    long err;
    int i;
    const char *const names[] = {
        "one", "two", "three", 0
    };
    char **values;
    const char *mypath = (random() & 1) ? path : filename;

    while (!done) {
        err = profile_init_path(mypath, &p);
        if (err) {
            com_err(prog, err, "calling profile_init(\"%s\")", mypath);
            exit(1);
        }
        for (i = 0; i < 10; i++) {
            values = 0;
            err = profile_get_values(p, names, &values);
            if (err == 0 && values != 0)
                profile_free_list(values);
        }
        profile_release(p);
    }
    return 0;
}

static void *modifier(void *arg)
{
    struct timespec req;
    while (!done) {
        req.tv_sec = 0;
        req.tv_nsec = random() & 499999999;
        nanosleep(&req, 0);
        utime(filename, 0);
/*      printf("."), fflush(stdout); */
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t thr;

    prog = argv[0];
    for (i = 0; i < nthreads; i++) {
        assert(0 == pthread_create(&thr, 0, worker, 0));
    }
    sleep(1);
    pthread_create(&thr, 0, modifier, 0);
    sleep(delay);
    done = 1;
    sleep(2);
    return 0;
}
