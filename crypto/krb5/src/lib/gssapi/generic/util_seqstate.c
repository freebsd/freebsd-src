/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/generic/util_seqstate.c - sequence number checking */
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
#include <string.h>

struct g_seqnum_state_st {
    /* Flags to indicate whether we are supposed to check for replays or
     * enforce strict sequencing. */
    int do_replay;
    int do_sequence;

    /* UINT32_MAX for 32-bit sequence numbers, UINT64_MAX for 64-bit.  Mask
     * against this after arithmetic to stay within the correct range. */
    uint64_t seqmask;

    /* The initial sequence number for this context.  This value will be
     * subtracted from all received sequence numbers to simplify wraparound. */
    uint64_t base;

    /* The expected next sequence number (one more than the highest previously
     * seen sequence number), relative to base. */
    uint64_t next;

    /*
     * A bitmap for the 64 sequence numbers prior to next.  If the 1<<(i-1) bit
     * is set, then we have seen seqnum next-i relative to base.  The least
     * significant bit is always set if we have received any sequence numbers,
     * and indicates the highest sequence number we have seen (next-1).  When
     * we advance next, we shift recvmap to the left.
     */
    uint64_t recvmap;
};

long
g_seqstate_init(g_seqnum_state *state_out, uint64_t seqnum, int do_replay,
                int do_sequence, int wide)
{
    g_seqnum_state state;

    *state_out = NULL;
    state = malloc(sizeof(*state));
    if (state == NULL)
        return ENOMEM;
    state->do_replay = do_replay;
    state->do_sequence = do_sequence;
    state->seqmask = wide ? UINT64_MAX : UINT32_MAX;
    state->base = seqnum;
    state->next = state->recvmap = 0;
    *state_out = state;
    return 0;
}

OM_uint32
g_seqstate_check(g_seqnum_state state, uint64_t seqnum)
{
    uint64_t rel_seqnum, offset, bit;

    if (!state->do_replay && !state->do_sequence)
        return GSS_S_COMPLETE;

    /* Use the difference from the base seqnum, to simplify wraparound. */
    rel_seqnum = (seqnum - state->base) & state->seqmask;

    if (rel_seqnum >= state->next) {
        /* seqnum is the expected sequence number or in the future.  Update the
         * received bitmap and expected next sequence number. */
        offset = rel_seqnum - state->next;
        state->recvmap = (state->recvmap << (offset + 1)) | 1;
        state->next = (rel_seqnum + 1) & state->seqmask;

        return (offset > 0 && state->do_sequence) ? GSS_S_GAP_TOKEN :
            GSS_S_COMPLETE;
    }

    /* seqnum is in the past.  Check if it's too old for replay detection. */
    offset = state->next - rel_seqnum;
    if (offset > 64)
        return state->do_sequence ? GSS_S_UNSEQ_TOKEN : GSS_S_OLD_TOKEN;

    /* Check for replay and mark as received. */
    bit = (uint64_t)1 << (offset - 1);
    if (state->do_replay && (state->recvmap & bit))
        return GSS_S_DUPLICATE_TOKEN;
    state->recvmap |= bit;

    return state->do_sequence ? GSS_S_UNSEQ_TOKEN : GSS_S_COMPLETE;
}

void
g_seqstate_free(g_seqnum_state state)
{
    free(state);
}

/*
 * These support functions are for the serialization routines
 */
void
g_seqstate_size(g_seqnum_state state, size_t *sizep)
{
    *sizep += sizeof(*state);
}

long
g_seqstate_externalize(g_seqnum_state state, unsigned char **buf,
                       size_t *lenremain)
{
    if (*lenremain < sizeof(*state))
        return ENOMEM;
    memcpy(*buf, state, sizeof(*state));
    *buf += sizeof(*state);
    *lenremain -= sizeof(*state);
    return 0;
}

long
g_seqstate_internalize(g_seqnum_state *state_out, unsigned char **buf,
                       size_t *lenremain)
{
    g_seqnum_state state;

    *state_out = NULL;
    if (*lenremain < sizeof(*state))
        return EINVAL;
    state = malloc(sizeof(*state));
    if (state == NULL)
        return ENOMEM;
    memcpy(state, *buf, sizeof(*state));
    *buf += sizeof(*state);
    *lenremain -= sizeof(*state);
    *state_out = state;
    return 0;
}
