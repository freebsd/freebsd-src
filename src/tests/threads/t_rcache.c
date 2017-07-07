/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/threads/t_rcache.c */
/*
 * Copyright (C) 2006 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include <com_err.h>
#include <krb5.h>
#include <pthread.h>

krb5_context ctx;
krb5_rcache rcache;
krb5_data piece = { .data = "hello", .length = 5 };
time_t end_time;
const char *prog;

struct tinfo {
    time_t now;
    unsigned long my_ctime;
    unsigned int my_cusec;
    unsigned int total;
    int idx;
};

#define DEFAULT_N_THREADS   2
#define DEFAULT_INTERVAL   20 /* 5 * 60 */

int init_once = 0;
int n_threads = DEFAULT_N_THREADS;
int interval = DEFAULT_INTERVAL;
int *ip;

static void wait_for_tick ()
{
    time_t now, next;
    now = time(0);
    do {
        next = time(0);
    } while (now == next);
}

static void try_one (struct tinfo *t)
{
    krb5_donot_replay r;
    krb5_error_code err;
    char buf[100], buf2[100];
    krb5_rcache my_rcache;

    snprintf(buf, sizeof(buf), "host/all-in-one.mit.edu/%p@ATHENA.MIT.EDU",
             buf);
    r.server = buf;
    r.client = (t->my_cusec & 7) + "abcdefgh@ATHENA.MIT.EDU";
    r.msghash = NULL;
    if (t->now != t->my_ctime) {
        if (t->my_ctime != 0) {
            snprintf(buf2, sizeof(buf2), "%3d: %ld %5d\n", t->idx,
                     t->my_ctime, t->my_cusec);
            printf("%s", buf2);
        }
        t->my_ctime = t->now;
        t->my_cusec = 1;
    } else
        t->my_cusec++;
    r.ctime = t->my_ctime;
    r.cusec = t->my_cusec;
    if (!init_once) {
        err = krb5_get_server_rcache(ctx, &piece, &my_rcache);
        if (err) {
            const char *msg = krb5_get_error_message(ctx, err);
            fprintf(stderr, "%s: %s while initializing replay cache\n", prog, msg);
            krb5_free_error_message(ctx, msg);
            exit(1);
        }
    } else
        my_rcache = rcache;
    err = krb5_rc_store(ctx, my_rcache, &r);
    if (err) {
        com_err(prog, err, "storing in replay cache");
        exit(1);
    }
    if (!init_once)
        krb5_rc_close(ctx, my_rcache);
}

static void *run_a_loop (void *x)
{
    struct tinfo t = { 0 };
/*    int chr = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"[(*(int*)x) % 27]; */

    t.now = time(0);
    t.idx = *(int *)x;
    while (t.now != time(0))
        ;
    t.now = time(0);
    while (t.now < end_time) {
        t.now = time(0);
        try_one(&t);
        t.total++;
#if 0
        printf("%c", chr);
        fflush(stdout);
#endif
    }
/*    printf("thread %u total %u\n", (unsigned) ((int *)x-ip), t.total);*/
    *(int*)x = t.total;
    return 0;
}

static void usage(void)
{
    fprintf (stderr, "usage: %s [ options ]\n", prog);
    fprintf (stderr, "options:\n");
    fprintf (stderr, "\t-1\tcreate one rcache handle for process\n");
    fprintf (stderr, "\t-t N\tnumber of threads to create (default: %d)\n",
             DEFAULT_N_THREADS);
    fprintf (stderr,
             "\t-i N\tinterval to run test over, in seconds (default: %d)\n",
             DEFAULT_INTERVAL);
    exit(1);
}

static const char optstring[] = "1t:i:";

static void process_options (int argc, char *argv[])
{
    int c;

    prog = argv[0];
    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case '?':
        case ':':
        default:
            usage ();
        case '1':
            init_once = 1;
            break;
        case 't':
            n_threads = atoi (optarg);
            if (n_threads < 1 || n_threads > 10000)
                usage ();
            break;
        case 'i':
            interval = atoi (optarg);
            if (interval < 2 || n_threads > 100000)
                usage ();
            break;
        }
    }
}

int main (int argc, char *argv[])
{
    krb5_error_code err;
    int i;
    unsigned long sum;

    process_options (argc, argv);
    err = krb5_init_context(&ctx);
    if (err) {
        com_err(prog, err, "initializing context");
        return 1;
    }

    /*
     * For consistency, run the tests without an existing replay
     * cache.  Since there isn't a way to ask the library for the
     * pathname that would be used for the rcache, we create an rcache
     * object and then destroy it.
     */
    err = krb5_get_server_rcache(ctx, &piece, &rcache);
    if (err) {
        const char *msg = krb5_get_error_message(ctx, err);
        fprintf(stderr, "%s: %s while initializing replay cache\n", prog, msg);
        krb5_free_error_message(ctx, msg);
        return 1;
    }
    err = krb5_rc_destroy(ctx, rcache);
    if (err) {
        const char *msg = krb5_get_error_message(ctx, err);
        fprintf(stderr, "%s: %s while destroying old replay cache\n",
                prog, msg);
        krb5_free_error_message(ctx, msg);
        return 1;
    }
    rcache = NULL;

    if (init_once) {
        err = krb5_get_server_rcache(ctx, &piece, &rcache);
        if (err) {
            const char *msg = krb5_get_error_message(ctx, err);
            fprintf(stderr, "%s: %s while initializing new replay cache\n",
                    prog, msg);
            krb5_free_error_message(ctx, msg);
            return 1;
        }
    }

    ip = malloc(sizeof(int) * n_threads);
    if (ip == 0 && n_threads > 0) {
        perror("malloc");
        exit(1);
    }
    for (i = 0; i < n_threads; i++)
        ip[i] = i;

    wait_for_tick ();
    end_time = time(0) + interval;

    for (i = 0; i < n_threads; i++) {
        pthread_t new_thread;
        int perr;
        perr = pthread_create(&new_thread, 0, run_a_loop, &ip[i]);
        if (perr) {
            errno = perr;
            perror("pthread_create");
            exit(1);
        }
    }
    while (time(0) < end_time + 1)
        sleep(1);
    sum = 0;
    for (i = 0; i < n_threads; i++) {
        sum += ip[i];
        printf("thread %d total %5d, about %.1f per second\n", i, ip[i],
               ((double) ip[i])/interval);
    }
    printf("total %lu in %d seconds, avg ~%.1f/sec, ~%.1f/sec/thread\n",
           sum, interval,
           ((double)sum)/interval, ((double)sum)/interval/n_threads);
    free(ip);

    if (init_once)
        krb5_rc_close(ctx, rcache);
    krb5_free_context(ctx);
    return 0;
}
