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
const char *rcname;
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

/* Encrypt data into out (preallocated by the caller) with a random key. */
static krb5_error_code encrypt_data (krb5_data *data, krb5_enc_data *out)
{
    krb5_keyblock kb;
    krb5_error_code err;

    err = krb5_c_make_random_key(ctx, ENCTYPE_AES256_CTS_HMAC_SHA1_96,
                                 &kb);
    if (err)
        return err;
    err = krb5_c_encrypt(ctx, &kb, KRB5_KEYUSAGE_TGS_REQ_AUTH, NULL, data,
                         out);
    krb5_free_keyblock_contents(ctx, &kb);
    return err;
}

static void try_one (struct tinfo *t)
{
    krb5_error_code err;
    char buf[256], buf2[512];
    krb5_rcache my_rcache;
    krb5_data d;
    krb5_enc_data enc;

    snprintf(buf, sizeof(buf), "host/all-in-one.mit.edu/%p@ATHENA.MIT.EDU",
             buf);

    /* k5_rc_store() requires a ciphertext.  Create one by encrypting a dummy
     * value in a random key. */
    d = string2data(buf);
    enc.ciphertext = make_data(buf2, sizeof(buf2));
    err = encrypt_data(&d, &enc);
    if (err != 0) {
        const char *msg = krb5_get_error_message(ctx, err);
        fprintf(stderr, "%s: encrypting authenticator: %s\n", prog, msg);
        krb5_free_error_message(ctx, msg);
        exit(1);
    }

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
    if (!init_once) {
        err = k5_rc_resolve(ctx, rcname, &my_rcache);
        if (err) {
            const char *msg = krb5_get_error_message(ctx, err);
            fprintf(stderr, "%s: %s while initializing replay cache\n", prog, msg);
            krb5_free_error_message(ctx, msg);
            exit(1);
        }
    } else
        my_rcache = rcache;
    err = k5_rc_store(ctx, my_rcache, &enc);
    if (err) {
        com_err(prog, err, "storing in replay cache");
        exit(1);
    }
    if (!init_once)
        k5_rc_close(ctx, my_rcache);
}

static void *run_a_loop (void *x)
{
    struct tinfo t = { 0 };

    t.now = time(0);
    t.idx = *(int *)x;
    while (t.now != time(0))
        ;
    t.now = time(0);
    while (t.now < end_time) {
        t.now = time(0);
        try_one(&t);
        t.total++;
    }
    *(int*)x = t.total;
    return 0;
}

static void usage(void)
{
    fprintf (stderr, "usage: %s [ options ] rcname\n", prog);
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

    argc -= optind;
    argv += optind;
    if (argc != 1)
        usage ();
    rcname = argv[0];
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

    if (init_once) {
        err = k5_rc_resolve(ctx, rcname, &rcache);
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
        k5_rc_close(ctx, rcache);
    krb5_free_context(ctx);
    return 0;
}
