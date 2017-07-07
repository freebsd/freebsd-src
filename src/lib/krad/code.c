/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/code.c - RADIUS code name table for libkrad */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "internal.h"

#include <string.h>

static const char *codes[UCHAR_MAX] = {
    "Access-Request",
    "Access-Accept",
    "Access-Reject",
    "Accounting-Request",
    "Accounting-Response",
    "Accounting-Status",
    "Password-Request",
    "Password-Ack",
    "Password-Reject",
    "Accounting-Message",
    "Access-Challenge",
    "Status-Server",
    "Status-Client",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Resource-Free-Request",
    "Resource-Free-Response",
    "Resource-Query-Request",
    "Resource-Query-Response",
    "Alternate-Resource-Reclaim-Request",
    "NAS-Reboot-Request",
    "NAS-Reboot-Response",
    NULL,
    "Next-Passcode",
    "New-Pin",
    "Terminate-Session",
    "Password-Expired",
    "Event-Request",
    "Event-Response",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Disconnect-Request",
    "Disconnect-Ack",
    "Disconnect-Nak",
    "Change-Filters-Request",
    "Change-Filters-Ack",
    "Change-Filters-Nak",
    NULL,
    NULL,
    NULL,
    NULL,
    "IP-Address-Allocate",
    "IP-Address-Release",
};

krad_code
krad_code_name2num(const char *name)
{
    unsigned char i;

    for (i = 0; i < UCHAR_MAX; i++) {
        if (codes[i] == NULL)
            continue;

        if (strcmp(codes[i], name) == 0)
            return ++i;
    }

    return 0;
}

const char *
krad_code_num2name(krad_code code)
{
    if (code == 0)
        return NULL;

    return codes[code - 1];
}
