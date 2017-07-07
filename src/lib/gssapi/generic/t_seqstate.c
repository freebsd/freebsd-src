/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/generic/t_seqstate.c - Test program for sequence number state */
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

#include "gssapiP_generic.h"

enum resultcode {
    NOERR = GSS_S_COMPLETE,
    GAP = GSS_S_GAP_TOKEN,
    UNSEQ = GSS_S_UNSEQ_TOKEN,
    OLD = GSS_S_OLD_TOKEN,
    REPLAY = GSS_S_DUPLICATE_TOKEN
};

enum replayflag { NO_REPLAY = 0, DO_REPLAY = 1 };
enum sequenceflag { NO_SEQUENCE = 0, DO_SEQUENCE = 1 };
enum width { NARROW = 0, WIDE = 1, BOTH = 2 };

struct test {
    uint64_t initial;
    enum replayflag do_replay;
    enum sequenceflag do_sequence;
    enum width wide_seqnums;
    size_t nseqs;
    struct {
        uint64_t seqnum;
        enum resultcode result;
    } seqs[10];
} tests[] = {
    /* No replay or sequence checking. */
    {
        10, NO_REPLAY, NO_SEQUENCE, BOTH,
        4, { { 11, NOERR }, { 10, NOERR }, { 10, NOERR }, { 9, NOERR } }
    },

    /* Basic sequence checking, no wraparound. */
    {
        100, NO_REPLAY, DO_SEQUENCE, BOTH,
        4, { { 100, NOERR }, { 102, GAP }, { 103, NOERR }, { 101, UNSEQ } }
    },

    /* Initial gap sequence checking, no wraparound. */
    {
        200, NO_REPLAY, DO_SEQUENCE, BOTH,
        4, { { 201, GAP }, { 202, NOERR }, { 200, UNSEQ }, { 203, NOERR } }
    },

    /* Sequence checking with wraparound. */
    {
        UINT32_MAX - 1, NO_REPLAY, DO_SEQUENCE, NARROW,
        4, { { UINT32_MAX - 1, NOERR }, { UINT32_MAX, NOERR }, { 0, NOERR },
             { 1, NOERR } }
    },
    {
        UINT32_MAX - 1, NO_REPLAY, DO_SEQUENCE, NARROW,
        4, { { UINT32_MAX - 1, NOERR }, { 0, GAP }, { UINT32_MAX, UNSEQ },
             { 1, NOERR } }
    },
    {
        UINT64_MAX - 1, NO_REPLAY, DO_SEQUENCE, WIDE,
        4, { { UINT64_MAX - 1, NOERR }, { UINT64_MAX, NOERR }, { 0, NOERR },
             { 1, NOERR } }
    },
    {
        UINT64_MAX - 1, NO_REPLAY, DO_SEQUENCE, WIDE,
        4, { { UINT64_MAX - 1, NOERR }, { 0, GAP }, { UINT64_MAX, UNSEQ },
             { 1, NOERR } }
    },

    /* 64-bit sequence checking beyond 32-bit range */
    {
        UINT32_MAX - 1, NO_REPLAY, DO_SEQUENCE, WIDE,
        4, { { UINT32_MAX - 1, NOERR },
             { UINT32_MAX, NOERR },
             { (uint64_t)UINT32_MAX + 1, NOERR },
             { (uint64_t)UINT32_MAX + 2, NOERR } }
    },
    {
        UINT32_MAX - 1, NO_REPLAY, DO_SEQUENCE, WIDE,
        4, { { UINT32_MAX - 1, NOERR },
             { (uint64_t)UINT32_MAX + 1, GAP },
             { UINT32_MAX, UNSEQ },
             { (uint64_t)UINT32_MAX + 2, NOERR } }
    },
    {
        UINT32_MAX - 1, NO_REPLAY, DO_SEQUENCE, WIDE,
        3, { { UINT32_MAX - 1, NOERR }, { UINT32_MAX, NOERR }, { 0, GAP } }
    },

    /* Replay without the replay flag set. */
    {
        250, NO_REPLAY, DO_SEQUENCE, BOTH,
        2, { { 250, NOERR }, { 250, UNSEQ } }
    },

    /* Basic replay detection with and without sequence checking. */
    {
        0, DO_REPLAY, DO_SEQUENCE, BOTH,
        10, { { 5, GAP }, { 3, UNSEQ }, { 8, GAP }, { 3, REPLAY },
              { 0, UNSEQ }, { 0, REPLAY }, { 5, REPLAY }, { 3, REPLAY },
              { 8, REPLAY }, { 9, NOERR } }
    },
    {
        0, DO_REPLAY, NO_SEQUENCE, BOTH,
        10, { { 5, NOERR }, { 3, NOERR }, { 8, NOERR }, { 3, REPLAY },
              { 0, NOERR }, { 0, REPLAY }, { 5, REPLAY }, { 3, REPLAY },
              { 8, REPLAY }, { 9, NOERR } }
    },

    /* Replay and sequence detection with wraparound.  The last seqnum produces
     * GAP because it is before the initial sequence number. */
    {
        UINT64_MAX - 5, DO_REPLAY, DO_SEQUENCE, WIDE,
        10, { { UINT64_MAX, GAP }, { UINT64_MAX - 2, UNSEQ }, { 0, NOERR },
              { UINT64_MAX, REPLAY }, { UINT64_MAX, REPLAY },
              { 2, GAP }, { 0, REPLAY }, { 1, UNSEQ },
              { UINT64_MAX - 2, REPLAY }, { UINT64_MAX - 6, GAP } }
    },
    {
        UINT32_MAX - 5, DO_REPLAY, DO_SEQUENCE, NARROW,
        10, { { UINT32_MAX, GAP }, { UINT32_MAX - 2, UNSEQ }, { 0, NOERR },
              { UINT32_MAX, REPLAY }, { UINT32_MAX, REPLAY },
              { 2, GAP }, { 0, REPLAY }, { 1, UNSEQ },
              { UINT32_MAX - 2, REPLAY }, { UINT32_MAX - 6, GAP } }
    },

    /* Old token edge cases.  The current code can detect replays up to 64
     * numbers behind the expected sequence number (1164 in this case). */
    {
        1000, DO_REPLAY, NO_SEQUENCE, BOTH,
        10, { { 1163, NOERR }, { 1100, NOERR }, { 1100, REPLAY },
              { 1163, REPLAY }, { 1099, OLD }, { 1100, REPLAY },
              { 1150, NOERR }, { 1150, REPLAY }, { 1000, OLD },
              { 999, NOERR } }
    },
};

int
main()
{
    size_t i, j;
    enum width w;
    struct test *t;
    g_seqnum_state seqstate;
    OM_uint32 status;

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        t = &tests[i];
        /* Try both widths if t->wide_seqnums is both, otherwise just one. */
        for (w = NARROW; w <= WIDE; w++) {
            if (t->wide_seqnums != BOTH && t->wide_seqnums != w)
                continue;
            if (g_seqstate_init(&seqstate, t->initial, t->do_replay,
                                t->do_sequence, w))
                abort();
            for (j = 0; j < t->nseqs; j++) {
                status = g_seqstate_check(seqstate, t->seqs[j].seqnum);
                if (status != t->seqs[j].result) {
                    fprintf(stderr, "Test %d seq %d failed: %d != %d\n",
                            (int)i, (int)j, status, t->seqs[j].result);
                    return 1;
                }
            }
            g_seqstate_free(seqstate);
        }
    }

    return 0;
}
