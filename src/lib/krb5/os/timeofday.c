/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/timeofday.c */
/*
 * Copyright 1990, 2007 by the Massachusetts Institute of Technology.
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

#include <time.h>

krb5_error_code KRB5_CALLCONV
krb5_timeofday(krb5_context context, register krb5_timestamp *timeret)
{
    krb5_os_context os_ctx;
    time_t tval;

    if (context == NULL)
        return EINVAL;

    os_ctx = &context->os_context;
    if (os_ctx->os_flags & KRB5_OS_TOFFSET_TIME) {
        *timeret = os_ctx->time_offset;
        return 0;
    }
    tval = time(0);
    if (tval == (time_t) -1)
        return (krb5_error_code) errno;
    if (os_ctx->os_flags & KRB5_OS_TOFFSET_VALID)
        tval += os_ctx->time_offset;
    *timeret = tval;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_check_clockskew(krb5_context context, krb5_timestamp date)
{
    krb5_timestamp currenttime;
    krb5_error_code retval;

    retval = krb5_timeofday(context, &currenttime);
    if (retval)
        return retval;
    if (!(labs((date)-currenttime) < context->clockskew))
        return KRB5KRB_AP_ERR_SKEW;

    return 0;
}
