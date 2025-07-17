/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/threads/gss-perf.c */
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
 * GSSAPI performance testing
 * initially contributed by Ken Raeburn
 */
/*
 * Possible to-do items:
 * - init-mutual testing (process msg back from accept)
 * - wrap/unwrap testing (one init/accept per thread, loop on wrap/unwrap)
 * - wrap/unwrap MT testing (one init/accept for process) ?
 * - init+accept with replay cache
 * - default to target "host@localhostname"
 * - input ccache option?
 *
 * Also, perhaps try to simulate certain application patterns, like
 * init/accept, exchange N messages with wrap/unwrap, destroy context,
 * all in a loop in M parallel threads.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <krb5.h>
#include <gssapi/gssapi.h>

#include <sys/time.h>
#include <sys/resource.h>

#define N_THREADS 2
#define ITER_COUNT 10000
static int init_krb5_first = 0;

struct resource_info {
    struct timeval start_time, end_time;
};
struct thread_info {
    pthread_t tid;
    struct resource_info r;
};

static gss_name_t target;
static char *prog, *target_name;
static unsigned int n_threads = N_THREADS;
static int iter_count = ITER_COUNT;
static int do_pause, do_mutual;
static int test_init, test_accept;

static void usage (void) __attribute__((noreturn));
static void set_target (char *);

static void
usage ()
{
    fprintf (stderr, "usage: %s [ options ] service-name\n", prog);
    fprintf (stderr, "  service-name\tGSSAPI host-based service name (e.g., 'host@FQDN')\n");
    fprintf (stderr, "options:\n");
    fprintf (stderr, "\t-I\ttest gss_init_sec_context\n");
    fprintf (stderr, "\t-A\ttest gss_accept_sec_context\n");
    fprintf (stderr, "\t-k K\tspecify keytab (remember FILE: or other prefix!)\n");
    fprintf (stderr, "\t-t N\tspecify number of threads (default %d)\n",
             N_THREADS);
    fprintf (stderr, "\t-i N\tset iteration count (default %d)\n",
             ITER_COUNT);
    fprintf (stderr, "\t-m\tenable mutual authentication flag (but don't do the additional calls)\n");
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

static char optstring[] = "k:t:i:KPmIA";

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

        case 'k':
            setenv ("KRB5_KTNAME", optarg, 1);
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

        case 'I':
            test_init = 1;
            break;
        case 'A':
            test_accept = 1;
            break;
        }
    }
    if (argc == optind + 1)
        set_target (argv[optind]);
    else
        usage ();

    if (test_init && test_accept) {
        fprintf (stderr, "-I and -A are mutually exclusive\n");
        usage ();
    }
    if (test_init == 0 && test_accept == 0)
        test_init = 1;
}

static void
display_a_status (const char *s_type, OM_uint32 type, OM_uint32 val)
{
    OM_uint32 mctx = 0;
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;

    do {
        maj_stat = gss_display_status (&min_stat,
                                       val,
                                       type,
                                       GSS_C_NO_OID,
                                       &mctx,
                                       &msg);
        if (maj_stat != GSS_S_COMPLETE) {
            fprintf (stderr,
                     "error getting display form of %s status code %#lx\n",
                     s_type, (unsigned long) val);
            exit (1);
        }
        fprintf (stderr, " %s: %.*s\n", s_type,
                 (int) msg.length, (char *) msg.value);
        gss_release_buffer (&min_stat, &msg);
    } while (mctx != 0);
}

static void
gss_error(const char *where, OM_uint32 maj_stat, OM_uint32 min_stat)
{
    fprintf (stderr, "%s: %s:\n", prog, where);
    display_a_status ("major", GSS_C_GSS_CODE, maj_stat);
    display_a_status ("minor", GSS_C_MECH_CODE, min_stat);
    exit (1);
}

static void
do_accept (gss_buffer_desc *msg, int iter)
{
    OM_uint32 maj_stat, min_stat;
    gss_name_t client = GSS_C_NO_NAME;
    gss_buffer_desc reply = GSS_C_EMPTY_BUFFER;
    gss_OID oid = GSS_C_NO_OID;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    OM_uint32 flags = do_mutual ? GSS_C_MUTUAL_FLAG : 0;

    reply.value = NULL;
    reply.length = 0;
    maj_stat = gss_accept_sec_context (&min_stat,
                                       &ctx,
                                       GSS_C_NO_CREDENTIAL,
                                       msg,
                                       GSS_C_NO_CHANNEL_BINDINGS,
                                       &client,
                                       &oid,
                                       &reply,
                                       &flags,
                                       NULL, /* time_rec */
                                       NULL); /* del_cred_handle */
    if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED) {
        fprintf (stderr, "pid %lu thread %#lx failing in iteration %d\n",
                 (unsigned long) getpid (), (unsigned long) pthread_self (),
                 iter);
        gss_error ("accepting context", maj_stat, min_stat);
    }
    gss_release_buffer (&min_stat, &reply);
    if (ctx != GSS_C_NO_CONTEXT)
        gss_delete_sec_context (&min_stat, &ctx, GSS_C_NO_BUFFER);
    gss_release_name (&min_stat, &client);
}

static gss_buffer_desc
do_init ()
{
    OM_uint32 maj_stat, min_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    OM_uint32 flags = 0, ret_flags = 0;
    gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;

    if (do_mutual)
        flags |= GSS_C_MUTUAL_FLAG;

    msg.value = NULL;
    msg.length = 0;
    maj_stat = gss_init_sec_context (&min_stat,
                                     GSS_C_NO_CREDENTIAL,
                                     &ctx,
                                     target,
                                     GSS_C_NO_OID,
                                     flags,
                                     0,
                                     NULL, /* no channel bindings */
                                     NULL, /* no previous token */
                                     NULL, /* ignore mech type */
                                     &msg,
                                     &ret_flags,
                                     NULL); /* time_rec */
    if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED) {
        gss_error ("initiating", maj_stat, min_stat);
    }
    if (ctx != GSS_C_NO_CONTEXT)
        gss_delete_sec_context (&min_stat, &ctx, GSS_C_NO_BUFFER);
    return msg;
}

static void
set_target (char *name)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc namebuf;

    target_name = name;
    namebuf.value = name;
    namebuf.length = strlen (name);
    maj_stat = gss_import_name (&min_stat,
                                &namebuf,
                                GSS_C_NT_HOSTBASED_SERVICE,
                                &target);
    if (maj_stat != GSS_S_COMPLETE)
        gss_error ("importing target name", maj_stat, min_stat);
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

static gss_buffer_desc init_msg;

static void run_iterations (struct resource_info *r)
{
    int i;
    OM_uint32 min_stat;

    r->start_time = now ();
    for (i = 0; i < iter_count; i++) {
        if (test_init) {
            gss_buffer_desc msg = do_init ();
            gss_release_buffer (&min_stat, &msg);
        } else if (test_accept) {
            do_accept (&init_msg, i);
        } else
            assert (test_init || test_accept);
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

    /* Probably should have a command-line option controlling this,
       but if a replay cache is used, we can't do just one
       init_sec_context and easily time just the accept_sec_context
       side.  */
    setenv ("KRB5RCACHETYPE", "none", 1);

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
    printf ("Test: %s  threads: %d  iterations: %d  target: %s\n",
            test_init ? "init" : "accept", n_threads, iter_count,
            target_name ? target_name : "(NONE)");
    if (do_pause) {
        printf ("pid %lu napping...\n", (unsigned long) getpid ());
        sleep (10);
    }
    /*
     * Some tests use one message and process it over and over.  Even
     * if not, this sort of "primes" things by fetching any needed
     * tickets just once.
     */
    init_msg = do_init ();
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
    return 0;
}
