/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/internal.h - SPAKE internal function declarations */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

#ifndef TRACE_H
#define TRACE_H

#include "k5-int.h"

/*
 * Possible improvements at the cost of more code:
 * - Groups could be displayed by name instead of number
 * - We could display the group list when tracing support messages
 */

#define TRACE_SPAKE_CLIENT_THASH(c, thash)                      \
    TRACE(c, "SPAKE final transcript hash: {hexdata}", thash)
#define TRACE_SPAKE_DERIVE_KEY(c, n, kb)                        \
    TRACE(c, "SPAKE derived K'[{int}] = {keyblock}", n, kb)
#define TRACE_SPAKE_KDC_THASH(c, thash)                         \
    TRACE(c, "SPAKE final transcript hash: {hexdata}", thash)
#define TRACE_SPAKE_KEYGEN(c, pubkey)                                   \
    TRACE(c, "SPAKE key generated with pubkey {hexdata}", pubkey)
#define TRACE_SPAKE_RECEIVE_CHALLENGE(c, group, pubkey)                 \
    TRACE(c, "SPAKE challenge received with group {int}, pubkey {hexdata}", \
          group, pubkey)
#define TRACE_SPAKE_RECEIVE_RESPONSE(c, pubkey)                         \
    TRACE(c, "SPAKE response received with pubkey {hexdata}", pubkey)
#define TRACE_SPAKE_RECEIVE_SUPPORT(c, group)                           \
    TRACE(c, "SPAKE support message received, selected group {int}", group)
#define TRACE_SPAKE_REJECT_CHALLENGE(c, group)                          \
    TRACE(c, "SPAKE challenge with group {int} rejected", (int)group)
#define TRACE_SPAKE_REJECT_SUPPORT(c)           \
    TRACE(c, "SPAKE support message rejected")
#define TRACE_SPAKE_RESULT(c, result)                           \
    TRACE(c, "SPAKE algorithm result: {hexdata}", result)
#define TRACE_SPAKE_SEND_CHALLENGE(c, group)                    \
    TRACE(c, "Sending SPAKE challenge with group {int}", group)
#define TRACE_SPAKE_SEND_RESPONSE(c)            \
    TRACE(c, "Sending SPAKE response")
#define TRACE_SPAKE_SEND_SUPPORT(c)             \
    TRACE(c, "Sending SPAKE support message")
#define TRACE_SPAKE_UNKNOWN_GROUP(c, name)                      \
    TRACE(c, "Unrecognized SPAKE group name: {str}", name)

#endif /* TRACE_H */
