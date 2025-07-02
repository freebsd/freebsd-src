/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/threads/init_ctx.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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
 * krb5 context creation performance testing
 * initially contributed by Ken Raeburn
 */

#include "k5-platform.h"
#include <unistd.h>
#include <pthread.h>
#include <krb5.h>

#include <sys/time.h>
#include <sys/resource.h>

#define N_THREADS 4
#define ITER_COUNT 40000
static int init_krb5_first = 0;

struct resource_info {
    struct timeval start_time, end_time;
};
struct thread_info {
    pthread_t tid;
    struct resource_info r;
};

static char *prog;
static unsigned int n_threads = N_THREADS;
static int iter_count = ITER_COUNT;
static int do_pause;

static void usage (void) __attribute__((noreturn));

static void
usage ()
{
    fprintf (stderr, "usage: %s [ options ]\n", prog);
    fprintf (stderr, "options:\n");
    fprintf (stderr, "\t-t N\tspecify number of threads (default %d)\n",
             N_THREADS);
    fprintf (stderr, "\t-i N\tset iteration count (default %d)\n",
             ITER_COUNT);
    fprintf (stderr, "\t-K\tinitialize a krb5_context for the duration\n");
    fprintf (stderr, "\t-P\tpause briefly after starting, to allow attaching dtrace/strace/etc\n");
    exit (1);
}

static int
numarg (char *arg)
{
    char *end;
    long val;

    val = strtol (arg, &end, 10);
    if (*arg == 0 || *end != 0) {
        fprintf (stderr, "invalid numeric argument '%s'\n", arg);
        usage ();
    }
    if (val >= 1 && val <= INT_MAX)
        return val;
    fprintf (stderr, "out of range numeric value %ld (1..%d)\n",
             val, INT_MAX);
    usage ();
}

static char optstring[] = "t:i:KP";

static void
process_options (int argc, char *argv[])
{
    int c;

    prog = strrchr (argv[0], '/');
    if (prog)
        prog++;
    else
        prog = argv[0];
    while ((c = getopt (argc, argv, optstring)) != -1) {
        switch (c) {
        case '?':
        case ':':
            usage ();
            break;

        case 't':
            n_threads = numarg (optarg);
            if (n_threads >= SIZE_MAX / sizeof (struct thread_info)) {
                n_threads = SIZE_MAX / sizeof (struct thread_info);
                fprintf (stderr, "limiting n_threads to %u\n", n_threads);
            }
            break;

        case 'i':
            iter_count = numarg (optarg);
            break;

        case 'K':
            init_krb5_first = 1;
            break;

        case 'P':
            do_pause = 1;
            break;
        }
    }
    if (argc != optind)
        usage ();
}

static long double
tvsub (struct timeval t1, struct timeval t2)
{
    /* POSIX says .tv_usec is signed.  */
    return (t1.tv_sec - t2.tv_sec
            + (long double) 1.0e-6 * (t1.tv_usec - t2.tv_usec));
}

static struct timeval
now (void)
{
    struct timeval tv;
    if (gettimeofday (&tv, NULL) < 0) {
        perror ("gettimeofday");
        exit (1);
    }
    return tv;
}

static void run_iterations (struct resource_info *r)
{
    int i;
    krb5_error_code err;
    krb5_context ctx;

    r->start_time = now ();
    for (i = 0; i < iter_count; i++) {
        err = krb5_init_context(&ctx);
        if (err) {
            com_err(prog, err, "initializing krb5 context");
            exit(1);
        }
        krb5_free_context(ctx);
    }
    r->end_time = now ();
}

static void *
thread_proc (void *p)
{
    run_iterations (p);
    return 0;
}

static struct thread_info *tinfo;

static krb5_context kctx;
static struct rusage start, finish;
static struct timeval start_time, finish_time;

int
main (int argc, char *argv[])
{
    long double user, sys, wallclock, total;
    unsigned int i;

    process_options (argc, argv);

    /*
     * Some places in the krb5 library cache data globally.
     * This option allows you to test the effect of that.
     */
    if (init_krb5_first && krb5_init_context (&kctx) != 0) {
        fprintf (stderr, "krb5_init_context error\n");
        exit (1);
    }
    tinfo = calloc (n_threads, sizeof (*tinfo));
    if (tinfo == NULL) {
        perror ("calloc");
        exit (1);
    }
    printf ("Threads: %d  iterations: %d\n", n_threads, iter_count);
    if (do_pause) {
        printf ("pid %lu napping...\n", (unsigned long) getpid ());
        sleep (10);
    }
    printf ("starting...\n");
    /* And *now* we start measuring the performance.  */
    if (getrusage (RUSAGE_SELF, &start) < 0) {
        perror ("getrusage");
        exit (1);
    }
    start_time = now ();
#define foreach_thread(IDXVAR) for (IDXVAR = 0; IDXVAR < n_threads; IDXVAR++)
    foreach_thread (i) {
        int err;

        err = pthread_create (&tinfo[i].tid, NULL, thread_proc, &tinfo[i].r);
        if (err) {
            fprintf (stderr, "pthread_create: %s\n", strerror (err));
            exit (1);
        }
    }
    foreach_thread (i) {
        int err;
        void *val;

        err = pthread_join (tinfo[i].tid, &val);
        if (err) {
            fprintf (stderr, "pthread_join: %s\n", strerror (err));
            exit (1);
        }
    }
    finish_time = now ();
    if (getrusage (RUSAGE_SELF, &finish) < 0) {
        perror ("getrusage");
        exit (1);
    }
    if (init_krb5_first)
        krb5_free_context (kctx);
    foreach_thread (i) {
        printf ("Thread %2d: elapsed time %Lfs\n", i,
                tvsub (tinfo[i].r.end_time, tinfo[i].r.start_time));
    }
    wallclock = tvsub (finish_time, start_time);
    /*
     * Report on elapsed time and CPU usage.  Depending what
     * performance issue you're chasing down, different values may be
     * of particular interest, so report all the info we've got.
     */
    printf ("Overall run time with %d threads = %Lfs, %Lfms per iteration.\n",
            n_threads, wallclock, 1000 * wallclock / iter_count);
    user = tvsub (finish.ru_utime, start.ru_utime);
    sys = tvsub (finish.ru_stime, start.ru_stime);
    total = user + sys;
    printf ("CPU usage:   user=%Lfs sys=%Lfs total=%Lfs.\n", user, sys, total);
    printf ("Utilization: user=%5.1Lf%% sys=%5.1Lf%% total=%5.1Lf%%\n",
            100 * user / wallclock,
            100 * sys / wallclock,
            100 * total / wallclock);
    printf ("Util/thread: user=%5.1Lf%% sys=%5.1Lf%% total=%5.1Lf%%\n",
            100 * user / wallclock / n_threads,
            100 * sys / wallclock / n_threads,
            100 * total / wallclock / n_threads);
    printf ("Total CPU use per iteration per thread: %Lfms\n",
            1000 * total / n_threads / iter_count);
    free(tinfo);
    return 0;
}
