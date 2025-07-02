/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_valid_times.c - test program for krb5int_validate_times() */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"

#define BOUNDARY (uint32_t)INT32_MIN

int
main()
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ticket_times times = { 0, 0, 0, 0 };

    ret = krb5_init_context(&context);
    assert(!ret);

    /* Current time is within authtime and end time. */
    ret = krb5_set_debugging_time(context, 1000, 0);
    times.authtime = 500;
    times.endtime = 1500;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is before starttime, but within clock skew. */
    times.starttime = 1100;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is before starttime by more than clock skew. */
    times.starttime = 1400;
    ret = krb5int_validate_times(context, &times);
    assert(ret == KRB5KRB_AP_ERR_TKT_NYV);

    /* Current time is after end time, but within clock skew. */
    times.starttime = 500;
    times.endtime = 800;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is after end time by more than clock skew. */
    times.endtime = 600;
    ret = krb5int_validate_times(context, &times);
    assert(ret == KRB5KRB_AP_ERR_TKT_EXPIRED);

    /* Current time is within starttime and endtime; current time and
     * endtime are across y2038 boundary. */
    ret = krb5_set_debugging_time(context, BOUNDARY - 100, 0);
    assert(!ret);
    times.starttime = BOUNDARY - 200;
    times.endtime = BOUNDARY + 500;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is before starttime, but by less than clock skew. */
    times.starttime = BOUNDARY + 100;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is before starttime by more than clock skew. */
    times.starttime = BOUNDARY + 250;
    ret = krb5int_validate_times(context, &times);
    assert(ret == KRB5KRB_AP_ERR_TKT_NYV);

    /* Current time is after endtime, but by less than clock skew. */
    ret = krb5_set_debugging_time(context, BOUNDARY + 100, 0);
    assert(!ret);
    times.starttime = BOUNDARY - 1000;
    times.endtime = BOUNDARY - 100;
    ret = krb5int_validate_times(context, &times);
    assert(!ret);

    /* Current time is after endtime by more than clock skew. */
    times.endtime = BOUNDARY - 300;
    ret = krb5int_validate_times(context, &times);
    assert(ret == KRB5KRB_AP_ERR_TKT_EXPIRED);

    krb5_free_context(context);

    return 0;
}
