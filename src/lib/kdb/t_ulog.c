/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/t_ulog.c - Unit tests for KDB update log */
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
 * This program performs unit tests for the update log functions in kdb_log.c.
 * Right now it contains only a test for issue #7839, checking that
 * ulog_add_update behaves appropriately when the last serial number is
 * reached.
 *
 * The test program accepts one argument, which it unlinks and then maps with
 * ulog_map().  This lets us test all of the update log functions except for
 * ulog_replay(), which needs to open and modify a Kerberos database.
 * ulog_replay is adequately exercised by the functional tests in t_iprop.py.
 */

#include "k5-int.h"
#include "kdb_log.h"

/* Use a zeroed context structure to avoid reading the profile.  This works
 * fine for the ulog functions. */
static struct _krb5_context context_st;
static krb5_context context = &context_st;

int
main(int argc, char **argv)
{
    kdb_log_context *lctx;
    kdb_hlog_t *ulog;
    kdb_incr_update_t upd;
    const char *filename;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        exit(1);
    }
    filename = argv[1];
    unlink(filename);

    if (ulog_map(context, filename, 10) != 0)
        abort();
    lctx = context->kdblog_context;
    ulog = lctx->ulog;

    /* Modify the ulog to look like it has reached the last serial number.
     * Leave the timestamps at 0 and don't bother setting up the entries. */
    ulog->kdb_num = lctx->ulogentries;
    ulog->kdb_last_sno = (kdb_sno_t)-1;
    ulog->kdb_first_sno = ulog->kdb_last_sno - ulog->kdb_num + 1;

    /* Add an empty update.  This should reinitialize the ulog, then add the
     * update with serial number 2. */
    memset(&upd, 0, sizeof(kdb_incr_update_t));
    if (ulog_add_update(context, &upd) != 0)
        abort();
    assert(ulog->kdb_num == 2);
    assert(ulog->kdb_first_sno == 1);
    assert(ulog->kdb_last_sno == 2);
    return 0;
}
