/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/t_replay.c - Test harness for replay cache */
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

#include "k5-int.h"

static void
usage(const char *progname)
{
    fprintf(stderr, "%s: Usage:\n", progname);
    fprintf(stderr, "  %s dump <filename>\n", progname);
    fprintf(stderr, "  %s store <rc> <cli> <srv> <msg> <tstamp> <usec>"
            " <now> <now-usec>\n", progname);
    fprintf(stderr, "  %s expunge <rc> <now> <now-usec>\n", progname);
    exit(1);
}

static char *
read_counted_string(FILE *fp)
{
    unsigned int len;
    char *str;

    if (fread(&len, sizeof(len), 1, fp) != 1)
        return NULL;
    if (len == 0 || len > 10000)
        return NULL;
    if ((str = malloc(len)) == NULL)
        return NULL;
    if (fread(str, 1, len, fp) != len)
        return NULL;
    if (str[len - 1] != 0)
        return NULL;
    return str;
}

static void
dump_rcache(const char *filename)
{
    FILE *fp;
    krb5_deltat lifespan;
    krb5_int16 vno;
    char *str;
    krb5_int32 usec;
    krb5_timestamp timestamp;

    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Can't open filename: %s\n", strerror(errno));
        return;
    }
    if (fread(&vno, sizeof(vno), 1, fp) != 1)
        return;
    if (fread(&lifespan, sizeof(lifespan), 1, fp) != 1)
        return;
    printf("Lifespan: %ld\n", (long) lifespan);
    while (1) {
        printf("---\n");

        if (!(str = read_counted_string(fp)))
            return;
        printf("Client: %s\n", str);
        free(str);

        if (!(str = read_counted_string(fp)))
            return;
        printf("Server: %s\n", str);
        free(str);

        if (fread(&usec, sizeof(usec), 1, fp) != 1)
            return;
        printf("Microseconds: %ld\n", (long) usec);

        if (fread(&timestamp, sizeof(timestamp), 1, fp) != 1)
            return;
        printf("Timestamp: %ld\n", (long) timestamp);
    }
}

static void
store(krb5_context ctx, char *rcspec, char *client, char *server, char *msg,
      krb5_timestamp timestamp, krb5_int32 usec, krb5_timestamp now_timestamp,
      krb5_int32 now_usec)
{
    krb5_rcache rc = NULL;
    krb5_error_code retval = 0;
    char *hash = NULL;
    krb5_donot_replay rep;
    krb5_data d;

    if (now_timestamp != 0)
        krb5_set_debugging_time(ctx, now_timestamp, now_usec);
    if ((retval = krb5_rc_resolve_full(ctx, &rc, rcspec)))
        goto cleanup;
    if ((retval = krb5_rc_recover_or_initialize(ctx, rc, ctx->clockskew)))
        goto cleanup;
    if (msg) {
        d.data = msg;
        d.length = strlen(msg);
        if ((retval = krb5_rc_hash_message(ctx, &d, &hash)))
            goto cleanup;
    }
    rep.client = client;
    rep.server = server;
    rep.msghash = hash;
    rep.cusec = usec;
    rep.ctime = timestamp;
    retval = krb5_rc_store(ctx, rc, &rep);
cleanup:
    if (retval == KRB5KRB_AP_ERR_REPEAT)
        printf("Replay\n");
    else if (!retval)
        printf("Entry successfully stored\n");
    else
        fprintf(stderr, "Failure: %s\n", krb5_get_error_message(ctx, retval));
    if (rc)
        krb5_rc_close(ctx, rc);
    if (hash)
        free(hash);
}

static void
expunge(krb5_context ctx, char *rcspec, krb5_timestamp now_timestamp,
        krb5_int32 now_usec)
{
    krb5_rcache rc = NULL;
    krb5_error_code retval = 0;

    if (now_timestamp > 0)
        krb5_set_debugging_time(ctx, now_timestamp, now_usec);
    if ((retval = krb5_rc_resolve_full(ctx, &rc, rcspec)))
        goto cleanup;
    if ((retval = krb5_rc_recover_or_initialize(ctx, rc, ctx->clockskew)))
        goto cleanup;
    retval = krb5_rc_expunge(ctx, rc);
cleanup:
    if (!retval)
        printf("Cache successfully expunged\n");
    else
        fprintf(stderr, "Failure: %s\n", krb5_get_error_message(ctx, retval));
    if (rc)
        krb5_rc_close(ctx, rc);
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_error_code retval;
    const char *progname;

    retval = krb5_init_context(&ctx);
    if (retval) {
        fprintf(stderr, "krb5_init_context returned error %ld\n",
                (long) retval);
        exit(1);
    }
    progname = argv[0];

    /* Parse arguments. */
    argc--; argv++;
    while (argc) {
        if (strcmp(*argv, "dump") == 0) {
            /*
             * Without going through the rcache interface, dump a
             * named dfl-format rcache file to stdout.  Takes a full
             * pathname argument.
             */
            const char *filename;

            argc--; argv++;
            if (!argc) usage(progname);
            filename = *argv;
            dump_rcache(filename);
        } else if (strcmp(*argv, "store") == 0) {
            /*
             * Using the rcache interface, store a replay record.
             * Takes an rcache spec like dfl:host as the first
             * argument.  If non-empty, the "msg" argument will be
             * hashed and provided in the replay record.  The
             * now-timestamp argument can be 0 to use the current
             * time.
             */
            char *rcspec, *client, *server, *msg;
            krb5_timestamp timestamp, now_timestamp;
            krb5_int32 usec, now_usec;

            argc--; argv++;
            if (!argc) usage(progname);
            rcspec = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            client = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            server = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            msg = (**argv) ? *argv : NULL;
            argc--; argv++;
            if (!argc) usage(progname);
            timestamp = (krb5_timestamp) atoll(*argv);
            argc--; argv++;
            if (!argc) usage(progname);
            usec = (krb5_int32) atol(*argv);
            argc--; argv++;
            if (!argc) usage(progname);
            now_timestamp = (krb5_timestamp) atoll(*argv);
            argc--; argv++;
            if (!argc) usage(progname);
            now_usec = (krb5_int32) atol(*argv);

            store(ctx, rcspec, client, server, msg, timestamp, usec,
                  now_timestamp, now_usec);
        } else if (strcmp(*argv, "expunge") == 0) {
            /*
             * Using the rcache interface, expunge a replay cache.
             * The now-timestamp argument can be 0 to use the current
             * time.
             */
            char *rcspec;
            krb5_timestamp now_timestamp;
            krb5_int32 now_usec;

            argc--; argv++;
            if (!argc) usage(progname);
            rcspec = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            now_timestamp = (krb5_timestamp) atoll(*argv);
            argc--; argv++;
            if (!argc) usage(progname);
            now_usec = (krb5_int32) atol(*argv);
            expunge(ctx, rcspec, now_timestamp, now_usec);
        } else
            usage(progname);
        argc--; argv++;
    }

    krb5_free_context(ctx);

    return 0;
}
