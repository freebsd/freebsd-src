/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/toffset.c - Manipulate time offset fields in os context */
/*
 * Copyright 1995, 2007 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

/*
 * This routine takes the "real time" as input, and sets the time
 * offset field in the context structure so that the krb5 time
 * routines will return the correct time as corrected by difference
 * between the system time and the "real time" as passed to this
 * routine
 *
 * If the real time microseconds are given as -1 the caller doesn't
 * know the microseconds value so the usec offset is always zero.
 */
krb5_error_code KRB5_CALLCONV
krb5_set_real_time(krb5_context context, krb5_timestamp seconds, krb5_int32 microseconds)
{
    krb5_os_context os_ctx = &context->os_context;
    krb5_int32 sec, usec;
    krb5_error_code retval;

    retval = krb5_crypto_us_timeofday(&sec, &usec);
    if (retval)
        return retval;

    os_ctx->time_offset = seconds - sec;
    os_ctx->usec_offset = (microseconds > -1) ? microseconds - usec : 0;

    os_ctx->os_flags = ((os_ctx->os_flags & ~KRB5_OS_TOFFSET_TIME) |
                        KRB5_OS_TOFFSET_VALID);
    return 0;
}

/*
 * This routine sets the krb5 time routines so that they will return
 * the seconds and microseconds value as input to this function.  This
 * is useful for running the krb5 routines through test suites
 */
krb5_error_code
krb5_set_debugging_time(krb5_context context, krb5_timestamp seconds, krb5_int32 microseconds)
{
    krb5_os_context os_ctx = &context->os_context;

    os_ctx->time_offset = seconds;
    os_ctx->usec_offset = microseconds;
    os_ctx->os_flags = ((os_ctx->os_flags & ~KRB5_OS_TOFFSET_VALID) |
                        KRB5_OS_TOFFSET_TIME);
    return 0;
}

/*
 * This routine turns off the time correction fields, so that the krb5
 * routines return the "natural" time.
 */
krb5_error_code
krb5_use_natural_time(krb5_context context)
{
    krb5_os_context os_ctx = &context->os_context;

    os_ctx->os_flags &= ~(KRB5_OS_TOFFSET_VALID|KRB5_OS_TOFFSET_TIME);

    return 0;
}

/*
 * This routine returns the current time offsets in use.
 */
krb5_error_code KRB5_CALLCONV
krb5_get_time_offsets(krb5_context context, krb5_timestamp *seconds, krb5_int32 *microseconds)
{
    krb5_os_context os_ctx = &context->os_context;

    if (seconds)
        *seconds = os_ctx->time_offset;
    if (microseconds)
        *microseconds = os_ctx->usec_offset;
    return 0;
}


/*
 * This routine sets the time offsets directly.
 */
krb5_error_code
krb5_set_time_offsets(krb5_context context, krb5_timestamp seconds, krb5_int32 microseconds)
{
    krb5_os_context os_ctx = &context->os_context;

    os_ctx->time_offset = seconds;
    os_ctx->usec_offset = microseconds;
    os_ctx->os_flags = ((os_ctx->os_flags & ~KRB5_OS_TOFFSET_TIME) |
                        KRB5_OS_TOFFSET_VALID);
    return 0;
}
